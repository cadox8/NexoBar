#pragma once

#include "../hospitality/Types.h"
#include "../Orm.h"
#include "Customer.h"
#include "User.h"

#include <optional>
#include <string>

namespace nexobar {
    /// An open tab/list for a customer or group; it is not tied to a dining table.
    class ConsumptionList {
    public:
        ConsumptionListId id{};
        std::optional<CustomerId> customerId;
        std::optional<std::string> customerName;
        std::string label;
        ListStatus status{ListStatus::Open};
        UserId openedBy{};
        Timestamp openedAt;
        std::optional<UserId> closedBy;
        std::optional<Timestamp> closedAt;
        std::optional<std::string> note;
        Cents total{};
    };

    using ConsumptionListTable = Nearon::Table<"lists", ConsumptionList,
        Nearon::Column<"id", &ConsumptionList::id, Nearon::PrimaryKey, Nearon::AutoIncrement>,
        Nearon::Column<"customer_id", &ConsumptionList::customerId, Nearon::ForeignKey<&Customer::id, Nearon::FkAction::SetNull>>,
        Nearon::Column<"customer_name", &ConsumptionList::customerName>,
        Nearon::Column<"label", &ConsumptionList::label>,
        Nearon::Column<"status", &ConsumptionList::status>,
        Nearon::Column<"opened_by", &ConsumptionList::openedBy, Nearon::ForeignKey<&User::id>>,
        Nearon::Column<"opened_at", &ConsumptionList::openedAt>,
        Nearon::Column<"closed_by", &ConsumptionList::closedBy, Nearon::ForeignKey<&User::id>>,
        Nearon::Column<"closed_at", &ConsumptionList::closedAt>,
        Nearon::Column<"note", &ConsumptionList::note>,
        Nearon::Column<"total_cents", &ConsumptionList::total, Nearon::Default<"0">>,
        Nearon::Index<"idx_lists_status", &ConsumptionList::status>,
        Nearon::Index<"idx_lists_customer", &ConsumptionList::customerId>>;
    NEARON_TABLE(ConsumptionListTable);
}
