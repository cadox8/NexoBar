#include "DailyCloseReceipt.h"

#include "../printer/TicketPrinter.h"
#include "../printer/PrinterDiagnostics.h"
#include "../template/TicketTemplate.h"
#include "CashDay.h"
#include "database/hospitality/Money.h"
#include "database/models/CashMovement.h"
#include "database/models/CashSession.h"
#include "database/models/Payment.h"
#include "database/models/Ticket.h"
#include "database/models/TicketLine.h"
#include "time/AppTime.h"

#include <boost/date_time/posix_time/posix_time.hpp>
#include <raylib.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>

namespace {
    std::filesystem::path closeTemplatePath() {
        const auto installed = std::filesystem::path(GetApplicationDirectory()) /
                               "resources" / "tickets" / "close.tpl";
        if (std::filesystem::is_regular_file(installed)) return installed;
        return std::filesystem::current_path() / "NexoBar" / "resources" / "tickets" / "close.tpl";
    }

    std::string quantityText(const std::int64_t quantity) {
        std::ostringstream output;
        output << std::fixed << std::setprecision(quantity % 1000 ? 3 : 0)
               << static_cast<double>(quantity) / 1000.0;
        return output.str();
    }

    std::string renderReport(Nearon::Database& database,
                             const std::vector<nexobar::Ticket>& tickets,
                             const std::vector<nexobar::Payment>& payments,
                             const std::vector<nexobar::CashSession>& sessions,
                             const std::string& period,
                             const std::string& title,
                             const std::string& businessName,
                             const platform::I18n& i18n,
                             const std::filesystem::path& requestedTemplatePath) {
        std::set<nexobar::TicketId> ticketIds;
        std::int64_t taxBase = 0, taxAmount = 0, grandTotal = 0, cash = 0, card = 0;
        for (const auto& ticket: tickets) {
            ticketIds.insert(ticket.id);
            taxBase += ticket.taxBase;
            taxAmount += ticket.taxAmount;
            grandTotal += ticket.total;
        }
        for (const auto& payment: payments)
            (payment.method == nexobar::PaymentMethod::Cash ? cash : card) += payment.amount;

        std::set<nexobar::CashSessionId> sessionIds;
        std::int64_t opening = 0, expected = 0, counted = 0, movementsIn = 0, movementsOut = 0;
        bool hasCountedAmount = false;
        for (const auto& session: sessions) {
            sessionIds.insert(session.id);
            opening += session.openingAmount;
            expected += session.expectedAmount.value_or(0);
            if (session.countedAmount) {
                counted += *session.countedAmount;
                hasCountedAmount = true;
            }
        }
        for (const auto& movement: database.selectAll<nexobar::CashMovement>()) {
            if (!sessionIds.contains(movement.sessionId)) continue;
            (movement.direction == nexobar::CashMovementDirection::In ? movementsIn : movementsOut) += movement.amount;
        }

        struct Total { std::int64_t quantity{}; std::int64_t amount{}; };
        std::map<std::string, Total> products;
        for (const auto& line: database.selectAll<nexobar::TicketLine>()) {
            if (!ticketIds.contains(line.ticketId)) continue;
            auto& value = products[line.description];
            value.quantity += line.quantity;
            value.amount += line.total;
        }

        const auto layout = PrinterDiagnostics::detectedReceiptLayout();
        const auto money = [&](const std::int64_t value) { return nexobar::formatMoney(value, i18n); };
        const auto row = [&](const std::string_view labelKey, const std::string& value) {
            return layout.leftRight(i18n.get(labelKey), value);
        };
        const std::int64_t average = tickets.empty() ? 0 : grandTotal / static_cast<std::int64_t>(tickets.size());
        const std::string countedText = hasCountedAmount ? money(counted) : "-";
        const std::string differenceText = hasCountedAmount ? money(counted - expected) : "-";
        TicketTemplate::TemplateData data;
        data.values = {
            {"business.name", layout.center(businessName)},
            {"close.title", layout.center(title)},
            {"close.date", layout.center(period)},
            {"close.period", period},
            {"close.ticket_count", std::to_string(tickets.size())},
            {"close.tax_base", money(taxBase)},
            {"close.tax_amount", money(taxAmount)},
            {"close.total", money(grandTotal)},
            {"close.cash", money(cash)},
            {"close.card", money(card)},
            {"close.average_ticket", money(average)},
            {"close.session_count", std::to_string(sessions.size())},
            {"close.opening_amount", money(opening)},
            {"close.expected_amount", money(expected)},
            {"close.counted_amount", countedText},
            {"close.difference", differenceText},
            {"close.movements_in", money(movementsIn)},
            {"close.movements_out", money(movementsOut)},
            {"close.ticket_count_row", row("settings.report_tickets", std::to_string(tickets.size()))},
            {"close.tax_base_row", row("settings.report_tax_base", money(taxBase))},
            {"close.tax_amount_row", row("settings.report_tax", money(taxAmount))},
            {"close.total_row", row("settings.report_total", money(grandTotal))},
            {"close.cash_row", row("settings.report_cash", money(cash))},
            {"close.card_row", row("settings.report_card", money(card))},
            {"close.average_ticket_row", row("reports.average_ticket", money(average))},
            {"close.session_count_row", row("reports.cash_sessions", std::to_string(sessions.size()))},
            {"close.opening_amount_row", row("reports.opening_amount", money(opening))},
            {"close.expected_amount_row", row("reports.expected_amount", money(expected))},
            {"close.counted_amount_row", row("reports.counted_amount", countedText)},
            {"close.difference_row", row("reports.cash_difference", differenceText)},
            {"close.movements_in_row", row("reports.movements_in", money(movementsIn))},
            {"close.movements_out_row", row("reports.movements_out", money(movementsOut))},
            {"separator", layout.separator()}
        };
        for (const auto& ticket: tickets) {
            TicketTemplate::TemplateData item;
            item.values = {
                {"close.ticket_number", ticket.fullNumber},
                {"close.ticket_total", money(ticket.total)},
                {"close.ticket_row", layout.leftRight(ticket.fullNumber, money(ticket.total))}
            };
            data.sections["tickets"].push_back(std::move(item));
        }
        for (const auto& [name, total]: products) {
            const auto quantity = quantityText(total.quantity);
            TicketTemplate::TemplateData item;
            item.values = {
                {"close.product_name", name},
                {"close.product_quantity", quantity},
                {"close.product_total", money(total.amount)},
                {"close.product_row", layout.left(name) + "\n" + layout.leftRight(quantity, money(total.amount))}
            };
            data.sections["products"].push_back(std::move(item));
        }

        const auto path = requestedTemplatePath.empty() ? closeTemplatePath() : requestedTemplatePath;
        std::ifstream input(path, std::ios::binary);
        if (input) {
            std::ostringstream source;
            source << input.rdbuf();
            return TicketTemplate::render(source.str(), data, &layout);
        }
        return data.values.at("business.name") + "\n" + data.values.at("close.title") + "\n" +
               data.values.at("close.date") + "\n" + data.values.at("separator") + "\n" +
               data.values.at("close.ticket_count_row") + "\n" + data.values.at("close.tax_base_row") + "\n" +
               data.values.at("close.tax_amount_row") + "\n" + data.values.at("close.total_row") + "\n" +
               data.values.at("close.cash_row") + "\n" + data.values.at("close.card_row") + "\n" +
               data.values.at("separator") + "\n\n";
    }
}

std::string DailyCloseReceipt::build(Nearon::Database& database,
                                     const boost::gregorian::date& date,
                                     const std::string& businessName,
                                     const platform::I18n& i18n,
                                     const std::filesystem::path& requestedTemplatePath) {
    std::vector<nexobar::Ticket> tickets;
    for (const auto& ticket: database.selectAll<nexobar::Ticket>())
        if (nexobar::ticketBelongsToCashDay(ticket, date)) tickets.push_back(ticket);
    std::vector<nexobar::Payment> payments;
    for (const auto& payment: database.selectAll<nexobar::Payment>())
        if (AppTime::toLocal(payment.createdAt).local_time().date() == date) payments.push_back(payment);
    std::vector<nexobar::CashSession> sessions;
    for (const auto& session: database.selectAll<nexobar::CashSession>())
        if (session.openedAt.date() == date) sessions.push_back(session);
    return renderReport(database, tickets, payments, sessions, boost::gregorian::to_iso_extended_string(date),
                        i18n.get("reports.close_title"), businessName, i18n, requestedTemplatePath);
}

std::string DailyCloseReceipt::buildReport(Nearon::Database& database,
                                           const std::vector<nexobar::Ticket>& tickets,
                                           const std::vector<nexobar::Payment>& payments,
                                           const std::vector<nexobar::CashSession>& sessions,
                                           const std::string& period,
                                           const std::string& businessName,
                                           const platform::I18n& i18n,
                                           const std::filesystem::path& templatePath) {
    return renderReport(database, tickets, payments, sessions, period, i18n.get("settings.report_header"),
                        businessName, i18n, templatePath);
}

std::size_t DailyCloseReceipt::close(Nearon::Database& database,
                                     const boost::gregorian::date& date,
                                     const nexobar::UserId closedBy) {
    return nexobar::closeCashDay(database, date, closedBy);
}

bool DailyCloseReceipt::print(Nearon::Database& database,
                              const boost::gregorian::date& date,
                              const std::string& businessName,
                              const platform::I18n& i18n,
                              std::string& error,
                              const std::filesystem::path& templatePath) {
    return TicketPrinter{}.printText(build(database, date, businessName, i18n, templatePath), error, i18n);
}
