#pragma once

#include "../hospitality/Types.h"
#include "../Orm.h"
#include "CashSession.h"
#include "ConsumptionList.h"
#include "User.h"

namespace nexobar {
    class Payment {
    public:
        PaymentId id{};
        ConsumptionListId listId{};
        CashSessionId cashSessionId{};
        PaymentMethod method{PaymentMethod::Cash};
        Cents amount{};
        Cents tendered{};
        Cents change{};
        UserId receivedBy{};
        Timestamp createdAt;
    };

    using PaymentTable = Nearon::Table<"payments", Payment,
        Nearon::Column<"id", &Payment::id, Nearon::PrimaryKey, Nearon::AutoIncrement>,
        Nearon::Column<"list_id", &Payment::listId, Nearon::ForeignKey<&ConsumptionList::id>>,
        Nearon::Column<"cash_session_id", &Payment::cashSessionId, Nearon::ForeignKey<&CashSession::id>>,
        Nearon::Column<"method", &Payment::method>,
        Nearon::Column<"amount_cents", &Payment::amount, Nearon::Check<"amount_cents > 0">>,
        Nearon::Column<"tendered_cents", &Payment::tendered>,
        Nearon::Column<"change_cents", &Payment::change, Nearon::Default<"0">>,
        Nearon::Column<"received_by", &Payment::receivedBy, Nearon::ForeignKey<&User::id>>,
        Nearon::Column<"created_at", &Payment::createdAt>,
        Nearon::Index<"idx_payments_list", &Payment::listId>>;
    NEARON_TABLE(PaymentTable);
}
