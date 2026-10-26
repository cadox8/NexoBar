#pragma once

#include "../hospitality/Types.h"
#include "../Orm.h"

#include <string>

namespace nexobar {
    /// IVA or other tax rate. Rates are snapshotted on list lines and tickets.
    class Tax {
    public:
        TaxId id{};
        std::string name;
        BasisPoints rate{};
        bool active{true};
    };

    using TaxTable = Nearon::Table<"taxes", Tax,
        Nearon::Column<"id", &Tax::id, Nearon::PrimaryKey, Nearon::AutoIncrement>,
        Nearon::Column<"name", &Tax::name, Nearon::Unique, Nearon::Collate<"NOCASE">>,
        Nearon::Column<"rate_bp", &Tax::rate, Nearon::Check<"rate_bp >= 0">>,
        Nearon::Column<"active", &Tax::active, Nearon::Default<"1">>>;
    NEARON_TABLE(TaxTable);
}
