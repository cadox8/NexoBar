#pragma once

#include "../hospitality/Types.h"
#include "../Orm.h"

#include <string>

namespace nexobar {
    /// A named capability such as "sell", "manage_products" or "void_ticket".
    class Permission {
    public:
        PermissionId id{};
        std::string key;
        std::string description;
        bool active{true};
    };

    using PermissionTable = Nearon::Table<"permissions", Permission,
        Nearon::Column<"id", &Permission::id, Nearon::PrimaryKey, Nearon::AutoIncrement>,
        Nearon::Column<"key", &Permission::key, Nearon::Unique, Nearon::Collate<"NOCASE">>,
        Nearon::Column<"description", &Permission::description, Nearon::Default<"''">>,
        Nearon::Column<"active", &Permission::active, Nearon::Default<"1">>>;
    NEARON_TABLE(PermissionTable);
}
