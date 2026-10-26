#include "StaffSettingsScreen.h"

#include "../../../NexoBar.h"
#include "SettingsWorkspaceScreen.h"

#include <algorithm>

StaffSettingsScreen::StaffSettingsScreen(std::function<void()> back)
    : m_back(std::move(back)),
      m_root(std::make_shared<BlockComponent>(LayoutAxis::Vertical,
                                              Color{24, 31, 42, 255})),
      m_title(std::make_shared<StaticText>("Empleados, roles y permisos")),
      m_description(
          std::make_shared<StaticText>("Selecciona qué quieres gestionar. Cada "
              "opción se abre en su propia vista.")) {
    m_font = NexoBarImp::get()->defaultFont();
    for (const auto& text: {m_title, m_description}) {
        text->font = m_font;
        text->wrap = true;
        text->color = RAYWHITE;
    }
    m_description->color = Color{170, 183, 202, 255};
    m_backRow = std::make_shared<SettingsActionRow>(
        "", m_font, std::vector<SettingsActionRow::Action>{
            {
                "Volver", [this] {
                    if (m_back)
                        m_back();
                }
            }
        });
    m_navigation = std::make_shared<SettingsActionRow>(
        "", m_font,
        std::vector<SettingsActionRow::Action>{
            {"Empleados", [this] { openUsers(); }},
            {"Roles", [this] { openRoles(); }},
            {"Permisos", [this] { openPermissions(); }}
        });
    m_root->content.add(m_backRow, LayoutSize::fill(), LayoutSize::px(42));
    m_root->content.add(m_title, LayoutSize::fill(), LayoutSize::px(58));
    m_root->content.add(m_description, LayoutSize::fill(), LayoutSize::px(50));
    m_root->content.add(m_navigation, LayoutSize::fill(), LayoutSize::px(58));
}

void StaffSettingsScreen::openUsers() {
    m_pages.setScreen("detail", std::make_shared<SettingsWorkspaceScreen>(
                          SettingsWorkspaceScreen::Kind::Users,
                          [this] { m_detailOpen = false; }));
    m_detailOpen = true;
}

void StaffSettingsScreen::openRoles() {
    m_pages.setScreen("detail", std::make_shared<SettingsWorkspaceScreen>(
                          SettingsWorkspaceScreen::Kind::Roles,
                          [this] { m_detailOpen = false; }));
    m_detailOpen = true;
}

void StaffSettingsScreen::openPermissions() {
    m_pages.setScreen("detail", std::make_shared<SettingsWorkspaceScreen>(
                          SettingsWorkspaceScreen::Kind::Permissions,
                          [this] { m_detailOpen = false; }));
    m_detailOpen = true;
}

void StaffSettingsScreen::layout() {
    const float unit = std::max(1.f, std::min(m_area.width, m_area.height));
    const float inset = std::clamp(unit * .04f, 14.f, 28.f);
    m_root->content.padding(LayoutInsets::all(inset)).gap(12);
    m_root->area(m_area);
    m_title->fontSize = std::clamp(unit * .058f, 24.f, 38.f);
    m_description->fontSize = std::clamp(unit * .03f, 15.f, 20.f);
    m_navigation->setFontSize(
        static_cast<int>(std::clamp(unit * .03f, 15.f, 19.f)));
    m_backRow->setFontSize(
        static_cast<int>(std::clamp(unit * .027f, 14.f, 18.f)));
}

void StaffSettingsScreen::update(float dt) {
    if (m_detailOpen) {
        m_pages.area(m_area);
        m_pages.update(dt);
        return;
    }
    layout();
    m_root->update(dt);
}

void StaffSettingsScreen::draw() {
    if (m_detailOpen) {
        m_pages.area(m_area);
        m_pages.draw();
        return;
    }
    layout();
    m_root->draw();
}
