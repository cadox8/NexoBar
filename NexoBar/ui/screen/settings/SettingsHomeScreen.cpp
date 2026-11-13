#include "SettingsHomeScreen.h"

#include "../../../NexoBar.h"
#include "SettingsWorkspaceScreen.h"
#include "SettingsScreens.h"
#include "StaffSettingsScreen.h"

#include <algorithm>

SettingsHomeScreen::SettingsHomeScreen(PermissionCheck hasPermission)
    : m_root(std::make_shared<BlockComponent>(LayoutAxis::Vertical,
                                              Color{24, 31, 42, 255})),
      m_title(std::make_shared<StaticText>()),
      m_subtitle(std::make_shared<StaticText>()) {
    NexoBarImp nexo;
    m_font = nexo->defaultFont();
    m_title->text = nexo->text("settings.title");
    m_subtitle->text = nexo->text("settings.subtitle");
    for (const auto& text: {m_title, m_subtitle}) {
        text->font = m_font;
        text->color = RAYWHITE;
    }
    m_subtitle->color = Color{159, 174, 196, 255};
    const auto back = [this] { m_pageOpen = false; };
    std::vector<SettingsActionRow::Action> actions;
    if (hasPermission("manage_users") || hasPermission("manage_roles") || hasPermission("manage_permissions"))
        actions.push_back({
            nexo->text("settings.staff"), [this, back] {
                open(std::make_shared<StaffSettingsScreen>(back));
            }
        });
    if (hasPermission("configure_business")) {
        actions.push_back({
            nexo->text("settings.business_short"), [this, back] {
                open(std::make_shared<BusinessSettingsScreen>(back));
            }
        });
    }
    if (hasPermission("edit_ticket_template")) actions.push_back({
            nexo->text("settings.template"), [this, back] {
                open(std::make_shared<TicketTemplateSettingsScreen>(back));
            }
        });
    if (hasPermission("configure_printer") || hasPermission("manage_cash"))
        actions.push_back({
            nexo->text("settings.status"), [this, back] {
                open(std::make_shared<SystemStatusSettingsScreen>(back));
            }
        });
    const auto split = std::min<std::size_t>(3, actions.size());
    m_hasSecondaryRow = actions.size() > split;
    std::vector primary(
        actions.begin(), actions.begin() + static_cast<std::ptrdiff_t>(split));
    std::vector secondary(
        actions.begin() + static_cast<std::ptrdiff_t>(split), actions.end());
    m_primaryRow =
            std::make_shared<SettingsActionRow>("", m_font, std::move(primary));
    m_secondaryRow =
            std::make_shared<SettingsActionRow>("", m_font, std::move(secondary));
    m_root->content.add(m_title, LayoutSize::fill(), LayoutSize::px(60));
    m_root->content.add(m_subtitle, LayoutSize::fill(), LayoutSize::px(38));
    m_root->content.add(m_primaryRow, LayoutSize::fill(), LayoutSize::px(58));
    if (m_hasSecondaryRow)
        m_root->content.add(m_secondaryRow, LayoutSize::fill(), LayoutSize::px(58));
}

void SettingsHomeScreen::open(const IScreenPtr& screen) {
    m_pages.setScreen("page", screen);
    m_pageOpen = true;
}

void SettingsHomeScreen::layout() {
    const float unit = std::max(1.f, std::min(m_area.width, m_area.height));
    const float inset = std::clamp(unit * .055f, 20.f, 42.f);
    m_root->content.padding(LayoutInsets::all(inset)).gap(14);
    m_root->area(m_area);
    m_title->fontSize = std::clamp(unit * .075f, 28.f, 46.f);
    m_subtitle->fontSize = std::clamp(unit * .032f, 15.f, 21.f);
    const int buttonFont = static_cast<int>(std::clamp(unit * .028f, 14.f, 18.f));
    m_primaryRow->setFontSize(buttonFont);
    if (m_hasSecondaryRow)
        m_secondaryRow->setFontSize(buttonFont);
}

void SettingsHomeScreen::update(float dt) {
    if (m_pageOpen) {
        m_pages.area(m_area);
        m_pages.update(dt);
        return;
    }
    layout();
    m_root->update(dt);
}

void SettingsHomeScreen::draw() {
    if (m_pageOpen) {
        m_pages.area(m_area);
        m_pages.draw();
        return;
    }
    layout();
    m_root->draw();
}
