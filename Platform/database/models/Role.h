#pragma once

#include "../hospitality/Types.h"
#include "../Orm.h"

#include <string>

namespace nexobar {
    class Role {
    public:
        RoleId id{};
        std::string name;
        std::string description;
        bool active{true};
    };

    using RoleTable = Nearon::Table<"roles", Role,
        Nearon::Column<"id", &Role::id, Nearon::PrimaryKey, Nearon::AutoIncrement>,
        Nearon::Column<"name", &Role::name, Nearon::Unique, Nearon::Collate<"NOCASE">>,
        Nearon::Column<"description", &Role::description, Nearon::Default<"''">>,
        Nearon::Column<"active", &Role::active, Nearon::Default<"1">>>;
    NEARON_TABLE(RoleTable);
}
