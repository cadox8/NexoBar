#include "DailyCloseReceipt.h"

#include "../printer/TicketPrinter.h"
#include "database/hospitality/Money.h"
#include "database/models/CashSession.h"
#include "database/models/Payment.h"
#include "database/models/Ticket.h"
#include "database/models/TicketLine.h"

#include <boost/date_time/posix_time/posix_time.hpp>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>

std::string DailyCloseReceipt::build(Nearon::Database& database,
                                     const boost::gregorian::date& date,
                                     const std::string& businessName,
                                     const platform::I18n& i18n) {
    std::set<nexobar::TicketId> ticketIds;
    std::int64_t grandTotal = 0;
    std::size_t ticketCount = 0;
    for (const auto& ticket : database.selectAll<nexobar::Ticket>()) {
        if (ticket.issuedAt.date() != date) continue;
        ticketIds.insert(ticket.id);
        grandTotal += ticket.total;
        ++ticketCount;
    }
    struct Total { std::int64_t quantity{}; std::int64_t amount{}; };
    std::map<std::string, Total> products;
    for (const auto& line : database.selectAll<nexobar::TicketLine>()) {
        if (!ticketIds.contains(line.ticketId)) continue;
        auto& value = products[line.description];
        value.quantity += line.quantity;
        value.amount += line.total;
    }

    std::ostringstream out;
    out << "\x1B" "a" "\x01" << businessName << '\n'
        << i18n.get("reports.close_title") << '\n'
        << boost::gregorian::to_iso_extended_string(date) << '\n'
        << "\x1B" "a" "\x00"
        << i18n.get("reports.separator") << '\n';
    for (const auto& [name, total] : products) {
        out << name << '\n' << "  " << std::fixed << std::setprecision(total.quantity % 1000 ? 3 : 0)
            << static_cast<double>(total.quantity) / 1000.0 << i18n.get("number.quantity_separator")
            << nexobar::formatMoney(total.amount, i18n) << '\n';
    }
    out << i18n.get("reports.separator") << '\n'
        << i18n.get("reports.ticket_count") << ticketCount << '\n'
        << i18n.get("receipt.total") << i18n.get("number.label_separator")
        << nexobar::formatMoney(grandTotal, i18n) << '\n'
        << i18n.get("reports.separator") << "\n\n";
    return out.str();
}

bool DailyCloseReceipt::print(Nearon::Database& database,
                              const boost::gregorian::date& date,
                              const std::string& businessName,
                              const platform::I18n& i18n,
                              const bool closeSessions,
                              const std::optional<nexobar::UserId> closedBy,
                              std::string& error) {
    TicketPrinter printer;
    if (!printer.printText(build(database, date, businessName, i18n), error, i18n)) return false;
    if (!closeSessions) return true;
    const auto payments = database.selectAll<nexobar::Payment>();
    auto sessions = database.selectAll<nexobar::CashSession>();
    for (auto& session : sessions) {
        if (session.closedAt || session.openedAt.date() != date) continue;
        std::int64_t cash = 0;
        for (const auto& payment : payments)
            if (payment.cashSessionId == session.id && payment.method == nexobar::PaymentMethod::Cash)
                cash += payment.amount;
        session.closedAt = boost::posix_time::second_clock::local_time();
        session.expectedAmount = session.openingAmount + cash;
        if (closedBy) session.closedBy = *closedBy;
        database.update(session);
    }
    return true;
}
