#include "RolePermissionsScreen.h"

#include "../../../NexoBar.h"
#include "database/models/RolePermission.h"

#include <algorithm>

RolePermissionsScreen::RolePermissionsScreen(std::function<void()> back)
    : m_root(std::make_shared<ScrollableBlock>(LayoutAxis::Vertical, Color{27, 34, 45, 255})),
      m_title(std::make_shared<StaticText>()), m_help(std::make_shared<StaticText>()),
      m_status(std::make_shared<StaticText>()), m_rolesDropdown(std::make_shared<DropdownComponent>()) {
    NexoBarImp nexo;
    m_font = nexo->defaultFont();
    for (const auto& text : {m_title, m_help, m_status}) { text->font = m_font; text->wrap = true; text->color = RAYWHITE; }
    m_title->text = nexo->text("settings.permissions");
    m_help->text = nexo->text("settings.permissions_help");
    m_help->color = Color{180, 192, 210, 255};
    m_status->color = Color{126, 225, 185, 255};
    m_rolesDropdown->font = m_font;
    m_rolesDropdown->onSelectionChanged = [this](int, const DropdownOption&) { rebuildChecks(); };
    m_back = std::make_shared<SettingsActionRow>("", m_font, std::vector<SettingsActionRow::Action>{
        {nexo->text("settings.back"), std::move(back)}
    });
    refresh();
}
void RolePermissionsScreen::refresh() {
    NexoBarImp nexo;
    m_roles = nexo->database()->query<nexobar::Role>()
        .where(Nearon::col<&nexobar::Role::active> == true).orderBy(Nearon::col<&nexobar::Role::name>.asc()).all();
    m_permissions = nexo->database()->query<nexobar::Permission>()
        .where(Nearon::col<&nexobar::Permission::active> == true).orderBy(Nearon::col<&nexobar::Permission::key>.asc()).all();
    std::vector<DropdownOption> options;
    for (const auto& role : m_roles) options.push_back({role.name, std::to_string(role.id.get())});
    m_rolesDropdown->setOptions(std::move(options));
    rebuildChecks();
}
void RolePermissionsScreen::rebuildChecks() {
    m_checks.clear();
    const auto* selection = m_rolesDropdown->selected();
    if (!selection) return;
    const nexobar::RoleId role{std::stoll(selection->value)};
    NexoBarImp nexo;
    for (const auto& permission : m_permissions) {
        const bool assigned = nexo->database()->count<nexobar::RolePermission>(
            Nearon::col<&nexobar::RolePermission::roleId> == role &&
            Nearon::col<&nexobar::RolePermission::permissionId> == permission.id) > 0;
        auto check = std::make_shared<CheckBox>(
            nexo->text("permissions." + permission.key), assigned);
        check->font = m_font;
        check->fontSize = 17;
        check->onChanged = [this, id = permission.id](const bool enabled) { setPermission(id, enabled); };
        m_checks.push_back(std::move(check));
    }
}
void RolePermissionsScreen::setPermission(const nexobar::PermissionId permission, const bool enabled) {
    const auto* selection = m_rolesDropdown->selected();
    if (!selection) return;
    const nexobar::RoleId role{std::stoll(selection->value)};
    NexoBarImp nexo;
    const auto condition = Nearon::col<&nexobar::RolePermission::roleId> == role &&
                           Nearon::col<&nexobar::RolePermission::permissionId> == permission;
    if (enabled && nexo->database()->count<nexobar::RolePermission>(condition) == 0) {
        nexobar::RolePermission value;
        value.roleId = role; value.permissionId = permission;
        nexo->database()->insert(value);
    } else if (!enabled) nexo->database()->removeWhere<nexobar::RolePermission>(condition);
    m_status->text = nexo->text("settings.permissions_saved");
}
void RolePermissionsScreen::layout() {
    const float unit = std::max(1.f, std::min(m_area.width, m_area.height));
    m_root->content.clear();
    m_root->content.padding(LayoutInsets::all(std::clamp(unit * .04f, 16.f, 32.f))).gap(10.f);
    m_root->content.add(m_back, LayoutSize::fill(), LayoutSize::px(44));
    m_root->content.add(m_title, LayoutSize::fill(), LayoutSize::px(56));
    m_root->content.add(m_help, LayoutSize::fill(), LayoutSize::px(58));
    m_root->content.add(m_rolesDropdown, LayoutSize::fill(), LayoutSize::px(48));
    for (const auto& check : m_checks) m_root->content.add(check, LayoutSize::fill(), LayoutSize::px(46));
    m_root->content.add(m_status, LayoutSize::fill(), LayoutSize::px(42));
    m_root->area(m_area);
    m_title->fontSize = std::clamp(unit * .06f, 25.f, 40.f);
    m_help->fontSize = m_status->fontSize = std::clamp(unit * .027f, 14.f, 19.f);
}
void RolePermissionsScreen::update(const float deltaSeconds) { layout(); m_root->update(deltaSeconds); }
void RolePermissionsScreen::draw() {
    layout();
    m_root->draw();
    // The popup must be rendered after the scrollable panel removes its scissor.
    m_rolesDropdown->drawOverlay();
}
