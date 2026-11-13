#pragma once

#include "../hospitality/Types.h"
#include "../Orm.h"
#include "Ticket.h"

namespace nexobar {
    class TicketTax {
    public:
        TicketTaxId id{};
        TicketId ticketId{};
        BasisPoints rate{};
        Cents taxBase{};
        Cents taxAmount{};
        std::string taxCode{"01"};
        std::string regimeCode{"01"};
        std::string operationCode{"S1"};
    };

    using TicketTaxTable = Nearon::Table<"ticket_taxes", TicketTax,
        Nearon::Column<"id", &TicketTax::id, Nearon::PrimaryKey, Nearon::AutoIncrement>,
        Nearon::Column<"ticket_id", &TicketTax::ticketId, Nearon::ForeignKey<&Ticket::id>>,
        Nearon::Column<"rate_bp", &TicketTax::rate>,
        Nearon::Column<"tax_base_cents", &TicketTax::taxBase>,
        Nearon::Column<"tax_cents", &TicketTax::taxAmount>,
        Nearon::Column<"tax_code", &TicketTax::taxCode, Nearon::Default<"'01'">>,
        Nearon::Column<"regime_code", &TicketTax::regimeCode, Nearon::Default<"'01'">>,
        Nearon::Column<"operation_code", &TicketTax::operationCode, Nearon::Default<"'S1'">>,
        Nearon::UniqueKey<&TicketTax::ticketId, &TicketTax::rate>>;
    NEARON_TABLE(TicketTaxTable);
}
