#pragma once

#include "../hospitality/Types.h"
#include "../Orm.h"
#include "User.h"

#include <optional>
#include <string>

namespace nexobar {
    class CashSession {
    public:
        CashSessionId id{};
        UserId openedBy{};
        Timestamp openedAt;
        Cents openingAmount{};
        std::optional<UserId> closedBy;
        std::optional<Timestamp> closedAt;
        std::optional<Cents> expectedAmount;
        std::optional<Cents> countedAmount;
        std::optional<std::string> note;
    };

    using CashSessionTable = Nearon::Table<"cash_sessions", CashSession,
        Nearon::Column<"id", &CashSession::id, Nearon::PrimaryKey, Nearon::AutoIncrement>,
        Nearon::Column<"opened_by", &CashSession::openedBy, Nearon::ForeignKey<&User::id>>,
        Nearon::Column<"opened_at", &CashSession::openedAt>,
        Nearon::Column<"opening_cents", &CashSession::openingAmount>,
        Nearon::Column<"closed_by", &CashSession::closedBy, Nearon::ForeignKey<&User::id>>,
        Nearon::Column<"closed_at", &CashSession::closedAt>,
        Nearon::Column<"expected_cents", &CashSession::expectedAmount>,
        Nearon::Column<"counted_cents", &CashSession::countedAmount>,
        Nearon::Column<"note", &CashSession::note>>;
    NEARON_TABLE(CashSessionTable);
}
