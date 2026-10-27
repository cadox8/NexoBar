#pragma once

#include "../hospitality/Types.h"
#include "../Orm.h"
#include "Role.h"

#include <string>

namespace nexobar {
    /// Staff account selected at the terminal. Optional PINs are stored as a
    /// salted derived value, never as clear text.
    class User {
    public:
        UserId id{};
        std::string displayName;
        RoleId roleId{};
        bool active{true};
        Timestamp createdAt;
        std::string pinSalt;
        std::string pinHash;
    };

    using UserTable = Nearon::Table<"users", User,
        Nearon::Column<"id", &User::id, Nearon::PrimaryKey, Nearon::AutoIncrement>,
        Nearon::Column<"display_name", &User::displayName, Nearon::Collate<"NOCASE">>,
        Nearon::Column<"role_id", &User::roleId, Nearon::ForeignKey<&Role::id>>,
        Nearon::Column<"active", &User::active, Nearon::Default<"1">>,
        Nearon::Column<"created_at", &User::createdAt>,
        Nearon::Column<"pin_salt", &User::pinSalt, Nearon::Default<"''">>,
        Nearon::Column<"pin_hash", &User::pinHash, Nearon::Default<"''">>>;
    NEARON_TABLE(UserTable);
}
