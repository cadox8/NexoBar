#pragma once

#include "Permission.h"
#include "Role.h"

namespace nexobar {
    class RolePermission {
    public:
        RolePermissionId id{};
        RoleId roleId{};
        PermissionId permissionId{};
    };

    using RolePermissionTable = Nearon::Table<"role_permissions", RolePermission,
        Nearon::Column<"id", &RolePermission::id, Nearon::PrimaryKey, Nearon::AutoIncrement>,
        Nearon::Column<"role_id", &RolePermission::roleId, Nearon::ForeignKey<&Role::id, Nearon::FkAction::Cascade>>,
        Nearon::Column<"permission_id", &RolePermission::permissionId, Nearon::ForeignKey<&Permission::id, Nearon::FkAction::Cascade>>,
        Nearon::UniqueKey<&RolePermission::roleId, &RolePermission::permissionId>>;
    NEARON_TABLE(RolePermissionTable);
}
