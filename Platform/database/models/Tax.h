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
        std::string taxCode{"01"};
        std::string regimeCode{"01"};
        std::string operationCode{"S1"};
    };

    using TaxTable = Nearon::Table<"taxes", Tax,
        Nearon::Column<"id", &Tax::id, Nearon::PrimaryKey, Nearon::AutoIncrement>,
        Nearon::Column<"name", &Tax::name, Nearon::Unique, Nearon::Collate<"NOCASE">>,
        Nearon::Column<"rate_bp", &Tax::rate, Nearon::Check<"rate_bp >= 0">>,
        Nearon::Column<"active", &Tax::active, Nearon::Default<"1">>,
        Nearon::Column<"tax_code", &Tax::taxCode, Nearon::Default<"'01'">>,
        Nearon::Column<"regime_code", &Tax::regimeCode, Nearon::Default<"'01'">>,
        Nearon::Column<"operation_code", &Tax::operationCode, Nearon::Default<"'S1'">>>;
    NEARON_TABLE(TaxTable);
}
