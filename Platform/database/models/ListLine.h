#pragma once

#include "../hospitality/Types.h"
#include "../Orm.h"
#include "ConsumptionList.h"
#include "Product.h"
#include "Tax.h"
#include "User.h"

#include <optional>
#include <string>

namespace nexobar {
    /// Product-backed or manually priced consumption. A null productId means a manual line.
    class ListLine {
    public:
        ListLineId id{};
        ConsumptionListId listId{};
        std::optional<ProductId> productId;
        std::string description;
        QuantityMilli quantity{1000};
        Cents unitPrice{};
        Cents discount{};
        TaxId taxId{};
        BasisPoints taxRate{};
        Cents total{};
        UserId addedBy{};
        Timestamp addedAt;
    };

    using ListLineTable = Nearon::Table<"list_lines", ListLine,
        Nearon::Column<"id", &ListLine::id, Nearon::PrimaryKey, Nearon::AutoIncrement>,
        Nearon::Column<"list_id", &ListLine::listId, Nearon::ForeignKey<&ConsumptionList::id, Nearon::FkAction::Cascade>>,
        Nearon::Column<"product_id", &ListLine::productId, Nearon::ForeignKey<&Product::id, Nearon::FkAction::SetNull>>,
        Nearon::Column<"description", &ListLine::description>,
        Nearon::Column<"quantity_milli", &ListLine::quantity, Nearon::Check<"quantity_milli > 0">>,
        Nearon::Column<"unit_price_cents", &ListLine::unitPrice, Nearon::Check<"unit_price_cents >= 0">>,
        Nearon::Column<"discount_cents", &ListLine::discount, Nearon::Default<"0">>,
        Nearon::Column<"tax_id", &ListLine::taxId, Nearon::ForeignKey<&Tax::id>>,
        Nearon::Column<"tax_rate_bp", &ListLine::taxRate>,
        Nearon::Column<"total_cents", &ListLine::total>,
        Nearon::Column<"added_by", &ListLine::addedBy, Nearon::ForeignKey<&User::id>>,
        Nearon::Column<"added_at", &ListLine::addedAt>,
        Nearon::Index<"idx_list_lines_list", &ListLine::listId>>;
    NEARON_TABLE(ListLineTable);
}
