#pragma once

#include "database/Database.h"
#include "database/models/CashSession.h"
#include "database/models/Payment.h"
#include "database/models/Ticket.h"
#include "time/AppTime.h"

#include <algorithm>
#include <charconv>
#include <optional>
#include <string_view>
#include <vector>

namespace nexobar {
    inline constexpr std::string_view CashCloseBoundaryPrefix =
        "NEXOBAR_CLOSE_THROUGH_TICKET=";

    inline std::optional<std::int64_t> cashCloseBoundary(const CashSession& session) {
        if (!session.note || !session.note->starts_with(CashCloseBoundaryPrefix))
            return std::nullopt;
        std::int64_t value = 0;
        const std::string_view text(*session.note);
        const auto first = text.data() + CashCloseBoundaryPrefix.size();
        const auto last = text.data() + text.size();
        const auto result = std::from_chars(first, last, value);
        return result.ec == std::errc{} && result.ptr == last ? std::optional{value} : std::nullopt;
    }

    inline Timestamp localTicketTime(const Ticket& ticket) {
        return AppTime::toLocal(ticket.issuedAt).local_time();
    }

    inline bool ticketBelongsToCashDay(const Ticket& ticket, const Date& date) {
        return localTicketTime(ticket).date() == date;
    }

    inline bool isTicketCashDayClosed(const std::vector<CashSession>& sessions,
                                      const Ticket& ticket) {
        const Timestamp issuedAt = localTicketTime(ticket);
        for (const auto& session : sessions) {
            if (session.openedAt.date() != issuedAt.date() || !session.closedAt) continue;
            if (const auto boundary = cashCloseBoundary(session)) {
                if (ticket.id.get() <= *boundary) return true;
            } else if (*session.closedAt >= issuedAt) {
                return true;
            }
        }
        return false;
    }

    inline bool isTicketCashDayClosed(Nearon::Database& database, const Ticket& ticket) {
        return isTicketCashDayClosed(database.selectAll<CashSession>(), ticket);
    }

    inline std::optional<Timestamp> latestCashDayClose(Nearon::Database& database,
                                                       const Date& date) {
        std::optional<Timestamp> latest;
        for (const auto& session : database.selectAll<CashSession>())
            if (session.openedAt.date() == date && session.closedAt &&
                (!latest || *session.closedAt > *latest))
                latest = session.closedAt;
        return latest;
    }

    inline std::size_t closeCashDay(Nearon::Database& database, const Date& date,
                                    const UserId closedBy) {
        return database.transaction([&] {
            const auto payments = database.selectAll<Payment>();
            auto sessions = database.selectAll<CashSession>();
            const auto tickets = database.selectAll<Ticket>();
            const auto closedAt = boost::posix_time::second_clock::local_time();
            std::size_t closed = 0;
            bool hasSessionForDay = false;
            for (auto& session : sessions) {
                if (session.openedAt.date() != date) continue;
                hasSessionForDay = true;
                if (session.closedAt) continue;
                Cents cash = 0;
                for (const auto& payment : payments)
                    if (payment.cashSessionId == session.id && payment.method == PaymentMethod::Cash)
                        cash += payment.amount;
                session.closedAt = closedAt;
                session.expectedAmount = session.openingAmount + cash;
                session.closedBy = closedBy;
                database.update(session);
                ++closed;
            }
            const bool hasPendingTickets = std::ranges::any_of(tickets, [&](const Ticket& ticket) {
                return ticketBelongsToCashDay(ticket, date) &&
                       !isTicketCashDayClosed(sessions, ticket);
            });
            if (!hasSessionForDay || hasPendingTickets) {
                std::int64_t lastTicketId = 0;
                for (const auto& ticket : tickets)
                    if (ticketBelongsToCashDay(ticket, date))
                        lastTicketId = std::max(lastTicketId, ticket.id.get());
                CashSession marker;
                marker.openedBy = closedBy;
                marker.openedAt = boost::posix_time::ptime(date);
                marker.openingAmount = 0;
                marker.closedBy = closedBy;
                marker.closedAt = closedAt;
                marker.expectedAmount = 0;
                marker.note = std::string(CashCloseBoundaryPrefix) + std::to_string(lastTicketId);
                database.insert(marker);
                ++closed;
            }
            return closed;
        });
    }
}
