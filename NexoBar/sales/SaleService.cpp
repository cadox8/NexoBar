#include "SaleService.h"

#include "database/Database.h"
#include "database/hospitality/HospitalityDatabase.h"
#include "database/hospitality/Money.h"
#include "database/models/TicketSeries.h"
#include "database/models/TicketTax.h"
#include "time/AppTime.h"

#include <boost/date_time/gregorian/gregorian.hpp>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>

namespace nexobar {
    IssuedSale SaleService::issue(const Business& business, const UserId user,
                                  const std::vector<SaleItem>& items) {
        if (items.empty())
            throw std::invalid_argument("No se puede emitir una venta vacía.");
        if (!m_database.exists<User>(user))
            throw std::invalid_argument("El empleado activo no existe.");

        return m_database.transaction([&] {
            const Timestamp issuedAt = AppTime::UTCNow();
            const int year = static_cast<int>(issuedAt.date().year());
            constexpr const char* seriesCode = "S";
            auto series = m_database.selectFirst<TicketSeries>(
                Nearon::col < &TicketSeries::code > == seriesCode &&
                Nearon::col < &TicketSeries::year > == year);
            if (!series) {
                TicketSeries created;
                created.code = seriesCode;
                created.year = year;
                created.description = "Tickets simplificados";
                created.id = m_database.insert(created);
                series = created;
            }
            if (!series->active)
                throw std::runtime_error("La serie de tickets del año está desactivada.");
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
            result.ticket.issuerName =
                    business.tradeName.empty() ? business.legalName : business.tradeName;
            result.ticket.issuerNif = business.nif;
            result.ticket.issuerAddress = business.address;

            std::map<BasisPoints, Cents> totalsByRate;
            std::int32_t lineNumber = 1;
            for (const auto& item: items) {
                if (item.quantity <= 0 || item.unitPrice < 0)
                    throw std::invalid_argument("La venta contiene una línea inválida.");
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
            for (const auto& [rate, gross]: totalsByRate) {
                const Cents base = baseFromGross(gross, rate);
                TicketTax tax;
                tax.ticketId = result.ticket.id;
                tax.rate = rate;
                tax.taxBase = base;
                tax.taxAmount = gross - base;
                m_database.insert(tax);
            }
            audit(m_database, user, "TICKET_ISSUED", "tickets", result.ticket.id.get(),
                  result.ticket.fullNumber);
            return result;
        });
    }
} // namespace nexobar
