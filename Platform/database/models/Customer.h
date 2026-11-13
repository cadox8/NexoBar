#pragma once

#include "../hospitality/Types.h"
#include "../Orm.h"

#include <optional>
#include <string>

namespace nexobar {
    class Customer {
    public:
        CustomerId id{};
        std::string name;
        std::optional<std::string> taxNumber;
        std::optional<std::string> email;
        std::optional<std::string> phone;
        std::optional<std::string> note;
        bool active{true};
    };

    using CustomerTable = Nearon::Table<"customers", Customer,
        Nearon::Column<"id", &Customer::id, Nearon::PrimaryKey, Nearon::AutoIncrement>,
        Nearon::Column<"name", &Customer::name, Nearon::Collate<"NOCASE">>,
        Nearon::Column<"tax_number", &Customer::taxNumber>,
        Nearon::Column<"email", &Customer::email>,
        Nearon::Column<"phone", &Customer::phone>,
        Nearon::Column<"note", &Customer::note>,
        Nearon::Column<"active", &Customer::active, Nearon::Default<"1">>>;
    NEARON_TABLE(CustomerTable);
}
