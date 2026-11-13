#pragma once

#include "../hospitality/Types.h"
#include "../Orm.h"

#include <string>

namespace nexobar {
    class TicketSeries {
    public:
        TicketSeriesId id{};
        std::string code;
        std::int32_t year{};
        std::int64_t lastNumber{};
        std::string description;
        bool active{true};
    };

    using TicketSeriesTable = Nearon::Table<"ticket_series", TicketSeries,
        Nearon::Column<"id", &TicketSeries::id, Nearon::PrimaryKey, Nearon::AutoIncrement>,
        Nearon::Column<"code", &TicketSeries::code>,
        Nearon::Column<"year", &TicketSeries::year>,
        Nearon::Column<"last_number", &TicketSeries::lastNumber, Nearon::Default<"0">>,
        Nearon::Column<"description", &TicketSeries::description, Nearon::Default<"''">>,
        Nearon::Column<"active", &TicketSeries::active, Nearon::Default<"1">>,
        Nearon::UniqueKey<&TicketSeries::code, &TicketSeries::year>>;
    NEARON_TABLE(TicketSeriesTable);
}
