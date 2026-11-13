#pragma once

#include "../hospitality/Types.h"
#include "../Orm.h"
#include "CashSession.h"
#include "User.h"

#include <string>

namespace nexobar {
    class CashMovement {
    public:
        CashMovementId id{};
        CashSessionId sessionId{};
        UserId userId{};
        CashMovementDirection direction{CashMovementDirection::In};
        Cents amount{};
        std::string reason;
        Timestamp createdAt;
    };

    using CashMovementTable = Nearon::Table<"cash_movements", CashMovement,
        Nearon::Column<"id", &CashMovement::id, Nearon::PrimaryKey, Nearon::AutoIncrement>,
        Nearon::Column<"session_id", &CashMovement::sessionId, Nearon::ForeignKey<&CashSession::id>>,
        Nearon::Column<"user_id", &CashMovement::userId, Nearon::ForeignKey<&User::id>>,
        Nearon::Column<"direction", &CashMovement::direction>,
        Nearon::Column<"amount_cents", &CashMovement::amount, Nearon::Check<"amount_cents > 0">>,
        Nearon::Column<"reason", &CashMovement::reason>,
        Nearon::Column<"created_at", &CashMovement::createdAt>,
        Nearon::Index<"idx_cash_movements_session", &CashMovement::sessionId>>;
    NEARON_TABLE(CashMovementTable);
}
