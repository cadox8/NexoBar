#pragma once

#include "../hospitality/Types.h"
#include "../Orm.h"
#include "Product.h"
#include "Ticket.h"

#include <optional>
#include <string>

namespace nexobar {
    class TicketLine {
    public:
        TicketLineId id{};
        TicketId ticketId{};
        std::int32_t lineNumber{};
        std::optional<ProductId> productId;
        std::string description;
        QuantityMilli quantity{1000};
        Cents unitPrice{};
        Cents discount{};
        BasisPoints taxRate{};
        Cents total{};
    };

    using TicketLineTable = Nearon::Table<"ticket_lines", TicketLine,
        Nearon::Column<"id", &TicketLine::id, Nearon::PrimaryKey, Nearon::AutoIncrement>,
        Nearon::Column<"ticket_id", &TicketLine::ticketId, Nearon::ForeignKey<&Ticket::id>>,
        Nearon::Column<"line_number", &TicketLine::lineNumber>,
        Nearon::Column<"product_id", &TicketLine::productId, Nearon::ForeignKey<&Product::id, Nearon::FkAction::SetNull>>,
        Nearon::Column<"description", &TicketLine::description>,
        Nearon::Column<"quantity_milli", &TicketLine::quantity>,
        Nearon::Column<"unit_price_cents", &TicketLine::unitPrice>,
        Nearon::Column<"discount_cents", &TicketLine::discount, Nearon::Default<"0">>,
        Nearon::Column<"tax_rate_bp", &TicketLine::taxRate>,
        Nearon::Column<"total_cents", &TicketLine::total>,
        Nearon::UniqueKey<&TicketLine::ticketId, &TicketLine::lineNumber>>;
    NEARON_TABLE(TicketLineTable);
}
