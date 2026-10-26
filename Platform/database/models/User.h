#pragma once

#include "../hospitality/Types.h"
#include "../Orm.h"
#include "Role.h"

#include <string>

namespace nexobar {
    /// Staff account selected at the terminal; no password is stored by this schema.
    class User {
    public:
        UserId id{};
        std::string displayName;
        RoleId roleId{};
        bool active{true};
        Timestamp createdAt;
    };

    using UserTable = Nearon::Table<"users", User,
        Nearon::Column<"id", &User::id, Nearon::PrimaryKey, Nearon::AutoIncrement>,
        Nearon::Column<"display_name", &User::displayName, Nearon::Collate<"NOCASE">>,
        Nearon::Column<"role_id", &User::roleId, Nearon::ForeignKey<&Role::id>>,
        Nearon::Column<"active", &User::active, Nearon::Default<"1">>,
        Nearon::Column<"created_at", &User::createdAt>>;
    NEARON_TABLE(UserTable);
}
