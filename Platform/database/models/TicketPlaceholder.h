#pragma once

#include "../hospitality/Types.h"
#include "../Orm.h"

#include <cstdint>
#include <string>

namespace nexobar {
    class TicketPlaceholder {
    public:
        TicketPlaceholderId id{};
        std::string placeholder;
        std::string descriptionKey;
        std::string templateSource;
        std::int64_t sortOrder{};
        bool active{true};
    };

    using TicketPlaceholderTable = Nearon::Table<"ticket_placeholders", TicketPlaceholder,
        Nearon::Column<"id", &TicketPlaceholder::id, Nearon::PrimaryKey, Nearon::AutoIncrement>,
        Nearon::Column<"placeholder", &TicketPlaceholder::placeholder, Nearon::Unique>,
        Nearon::Column<"description_key", &TicketPlaceholder::descriptionKey>,
        Nearon::Column<"template_source", &TicketPlaceholder::templateSource>,
        Nearon::Column<"sort_order", &TicketPlaceholder::sortOrder, Nearon::Default<"0">>,
        Nearon::Column<"active", &TicketPlaceholder::active, Nearon::Default<"1">>,
        Nearon::Index<"idx_ticket_placeholders_order", &TicketPlaceholder::sortOrder>>;
    NEARON_TABLE(TicketPlaceholderTable);
}
