#pragma once

#include "../../components/controls/CheckBox.h"
#include "../../components/controls/DropdownComponent.h"
#include "../../components/layout/ScrollableBlock.h"
#include "../../components/settings/SettingsActionRow.h"
#include "../../components/text/StaticText.h"
#include "../IScreen.h"
#include "database/models/Permission.h"
#include "database/models/Role.h"

#include <functional>
#include <vector>

class RolePermissionsScreen final : public IScreen {
public:
    explicit RolePermissionsScreen(std::function<void()> back);
    void update(float deltaSeconds) override;
    void draw() override;
private:
    void refresh();
    void rebuildChecks();
    void setPermission(nexobar::PermissionId permission, bool enabled);
    void layout();
    ScrollableBlockPtr m_root;
    StaticTextPtr m_title, m_help, m_status;
    DropdownComponentPtr m_rolesDropdown;
    SettingsActionRowPtr m_back;
    std::vector<nexobar::Role> m_roles;
    std::vector<nexobar::Permission> m_permissions;
    std::vector<CheckBoxPtr> m_checks;
};
