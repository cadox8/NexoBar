#include "SaleService.h"

#include "database/Database.h"
#include "database/hospitality/HospitalityDatabase.h"
#include "database/hospitality/Money.h"
#include "database/models/TicketSeries.h"
#include "database/models/TicketTax.h"
#include "time/AppTime.h"
#include "fiscal/FiscalService.h"

#include <boost/date_time/gregorian/gregorian.hpp>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>

namespace nexobar {
    IssuedSale SaleService::issue(const Business& business, const UserId user,
                                  const std::vector<SaleItem>& items,
                                  const std::optional<ConsumptionListId> listId) {
        if (items.empty())
            throw HospitalityError("errors.sale_empty");
        if (!m_database.exists<User>(user))
            throw HospitalityError("errors.active_user_missing");

        return m_database.transaction([&] {
            const Timestamp issuedAt = AppTime::UTCNow();
            const auto localIssuedAt = AppTime::toLocal(issuedAt).local_time();
            std::optional<ConsumptionList> sourceList;
            if (listId) {
                sourceList = m_database.get<ConsumptionList>(*listId);
                if (sourceList->status != ListStatus::Open)
                    throw HospitalityError("errors.list_not_open");
            }
            const int year = static_cast<int>(localIssuedAt.date().year());
            constexpr const char* seriesCode = "S";
            auto series = m_database.selectFirst<TicketSeries>(
                Nearon::col < &TicketSeries::code > == seriesCode &&
                Nearon::col < &TicketSeries::year > == year);
            if (!series) {
                TicketSeries created;
                created.code = seriesCode;
                created.year = year;
                created.description = m_text("ticket_series.simplified");
                created.id = m_database.insert(created);
                series = created;
            }
            if (!series->active)
                throw HospitalityError("errors.ticket_series_inactive");
            ++series->lastNumber;
            m_database.update(*series);

            std::ostringstream fullNumber;
            fullNumber << series->code << '-' << year << '-' << std::setw(6)
                    << std::setfill('0') << series->lastNumber;

            IssuedSale result;
            result.ticket.seriesId = series->id;
            result.ticket.seriesCode = series->code;
            result.ticket.number = series->lastNumber;
            result.ticket.fullNumber = fullNumber.str();
            result.ticket.type = TicketType::Simplified;
            result.ticket.issuedAt = issuedAt;
            result.ticket.issuedBy = user;
            result.ticket.listId = listId;
            if (sourceList)
                result.ticket.customerName = sourceList->customerName.value_or(sourceList->label);
            if (listId) {
                const auto sourceLine = m_database.query<ListLine>()
                    .where(Nearon::col<&ListLine::listId> == *listId)
                    .orderBy(Nearon::col<&ListLine::addedAt>.desc()).first();
                if (sourceLine) {
                    const auto operationDate =
                        AppTime::toLocal(sourceLine->addedAt).local_time().date();
                    if (operationDate != localIssuedAt.date())
                        result.ticket.operationDate = operationDate;
                }
            }
            result.ticket.issuerName = business.legalName;
            result.ticket.issuerNif = business.nif;
            result.ticket.issuerAddress = business.address;

            std::map<BasisPoints, Cents> totalsByRate;
            std::map<BasisPoints, SaleItem> fiscalTaxByRate;
            std::int32_t lineNumber = 1;
            for (const auto& item: items) {
                if (item.quantity <= 0 || item.unitPrice < 0)
                    throw HospitalityError("errors.sale_line_invalid");
                TicketLine line;
                line.lineNumber = lineNumber++;
                line.productId = item.productId;
                line.description = item.description;
                line.quantity = item.quantity;
                line.unitPrice = item.unitPrice;
                line.taxRate = item.taxRate;
                line.total = lineTotal(item.unitPrice, item.quantity);
                result.ticket.total += line.total;
                totalsByRate[line.taxRate] += line.total;
                fiscalTaxByRate.try_emplace(line.taxRate, item);
                result.lines.push_back(std::move(line));
            }
            for (const auto& [rate, gross]: totalsByRate) {
                const Cents base = baseFromGross(gross, rate);
                result.ticket.taxBase += base;
                result.ticket.taxAmount += gross - base;
            }
            result.ticket.id = m_database.insert(result.ticket);
            for (auto& line: result.lines) {
                line.ticketId = result.ticket.id;
                line.id = m_database.insert(line);
            }
            std::vector<TicketTax> ticketTaxes;
            for (const auto& [rate, gross]: totalsByRate) {
                const Cents base = baseFromGross(gross, rate);
                TicketTax tax;
                tax.ticketId = result.ticket.id;
                tax.rate = rate;
                tax.taxBase = base;
                tax.taxAmount = gross - base;
                const auto& fiscalTax = fiscalTaxByRate.at(rate);
                tax.taxCode = fiscalTax.taxCode;
                tax.regimeCode = fiscalTax.regimeCode;
                tax.operationCode = fiscalTax.operationCode;
                m_database.insert(tax);
                ticketTaxes.push_back(tax);
            }
            FiscalService fiscal(m_database, m_text);
            fiscal.recordIssue(business, result.ticket, ticketTaxes, user);
            if (sourceList) {
                auto& list = *sourceList;
                list.status = ListStatus::Closed;
                list.closedBy = user;
                list.closedAt = issuedAt;
                m_database.update(list);
                audit(m_database, user, "LIST_CLOSED", "lists", list.id.get(),
                      list.label);
            }
            audit(m_database, user, "TICKET_ISSUED", "tickets", result.ticket.id.get(),
                  result.ticket.fullNumber);
            return result;
        });
    }
} // namespace nexobar
