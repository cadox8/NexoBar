#pragma once

#include "../hospitality/Types.h"
#include "../Orm.h"
#include "ConsumptionList.h"
#include "Customer.h"
#include "TicketSeries.h"
#include "User.h"

#include <optional>
#include <string>

namespace nexobar {
    /// Immutable issued document. Corrections are represented by another rectifying ticket.
    class Ticket {
    public:
        TicketId id{};
        TicketSeriesId seriesId{};
        std::string seriesCode;
        std::int64_t number{};
        std::string fullNumber;
        TicketType type{TicketType::Simplified};
        Timestamp issuedAt;
        std::optional<Date> operationDate;
        std::optional<ConsumptionListId> listId;
        UserId issuedBy{};
        std::string issuerName;
        std::string issuerNif;
        std::string issuerAddress;
        std::optional<CustomerId> customerId;
        std::optional<std::string> customerName;
        std::optional<std::string> customerTaxNumber;
        std::optional<std::string> customerAddress;
        Cents taxBase{};
        Cents taxAmount{};
        Cents total{};
        std::optional<TicketId> rectifiesTicketId;
        std::optional<std::string> rectificationReason;
    };

    using TicketTable = Nearon::Table<"tickets", Ticket,
        Nearon::Column<"id", &Ticket::id, Nearon::PrimaryKey, Nearon::AutoIncrement>,
        Nearon::Column<"series_id", &Ticket::seriesId, Nearon::ForeignKey<&TicketSeries::id>>,
        Nearon::Column<"series_code", &Ticket::seriesCode>,
        Nearon::Column<"number", &Ticket::number, Nearon::Check<"number > 0">>,
        Nearon::Column<"full_number", &Ticket::fullNumber, Nearon::Unique>,
        Nearon::Column<"type", &Ticket::type>,
        Nearon::Column<"issued_at", &Ticket::issuedAt>,
        Nearon::Column<"operation_date", &Ticket::operationDate>,
        Nearon::Column<"list_id", &Ticket::listId, Nearon::ForeignKey<&ConsumptionList::id, Nearon::FkAction::SetNull>>,
        Nearon::Column<"issued_by", &Ticket::issuedBy, Nearon::ForeignKey<&User::id>>,
        Nearon::Column<"issuer_name", &Ticket::issuerName>,
        Nearon::Column<"issuer_nif", &Ticket::issuerNif>,
        Nearon::Column<"issuer_address", &Ticket::issuerAddress>,
        Nearon::Column<"customer_id", &Ticket::customerId, Nearon::ForeignKey<&Customer::id, Nearon::FkAction::SetNull>>,
        Nearon::Column<"customer_name", &Ticket::customerName>,
        Nearon::Column<"customer_tax_number", &Ticket::customerTaxNumber>,
        Nearon::Column<"customer_address", &Ticket::customerAddress>,
        Nearon::Column<"tax_base_cents", &Ticket::taxBase>,
        Nearon::Column<"tax_cents", &Ticket::taxAmount>,
        Nearon::Column<"total_cents", &Ticket::total>,
        Nearon::Column<"rectifies_ticket_id", &Ticket::rectifiesTicketId, Nearon::ForeignKey<&Ticket::id>>,
        Nearon::Column<"rectification_reason", &Ticket::rectificationReason>,
        Nearon::UniqueKey<&Ticket::seriesId, &Ticket::number>,
        Nearon::Index<"idx_tickets_issued_at", &Ticket::issuedAt>>;
    NEARON_TABLE(TicketTable);
}
