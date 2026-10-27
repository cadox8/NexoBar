#include "MainScreen.h"

#include "../../../NexoBar.h"
#include "../menus/HeaderScreen.h"
#include "../reports/ReportScreen.h"
#include "../sales/ProductScreen.h"
#include "../settings/SettingsHomeScreen.h"

MainScreen::MainScreen(std::shared_ptr<ProductState> state, std::function<void()> logout)
    : ScreenLayout(LayoutAxis::Vertical), m_content(std::make_shared<ScreenLayout>(LayoutAxis::Vertical)) {
    NexoBarImp nexo;
    auto sales = std::make_shared<ProductScreen>(state);
    auto reports = std::make_shared<ReportScreen>(nexobar::UserId{state->userId},
        state->hasPermission("print_reports"), state->hasPermission("manage_cash"));
    auto settings = std::make_shared<SettingsHomeScreen>(
        [state](const std::string& permission) { return state->hasPermission(permission); });
    m_content->addWeighted("page", sales);

    std::vector<SideMenuScreen::Item> items;
    items.push_back({"sales", nexo->text("menu.sales"), "icon-cart", [this, sales] {
        m_content->setScreen("page", sales); m_menu->select("sales");
    }});
    items.push_back({"reports", nexo->text("menu.reports"), "icon-reports", [this, reports] {
        m_content->setScreen("page", reports); m_menu->select("reports");
    }, false, state->hasPermission("view_reports")});
    items.push_back({"settings", nexo->text("menu.settings"), "icon-settings", [this, settings] {
        m_content->setScreen("page", settings); m_menu->select("settings");
    }, true, state->canOpenSettings()});
    items.push_back({"logout", nexo->text("menu.logout"), "icon-logout", std::move(logout), true, true});
    m_menu = std::make_shared<SideMenuScreen>(std::move(items));

    auto body = std::make_shared<ScreenLayout>(LayoutAxis::Horizontal);
    body->gap(8.f);
    LayoutOptions menuOptions;
    menuOptions.width = LayoutSize::px(58.f);
    body->addScreen("menu", m_menu, menuOptions);
    body->addWeighted("content", m_content);

    LayoutOptions headerOptions;
    headerOptions.height = LayoutSize::px(58.f);
    addScreen("header", std::make_shared<HeaderScreen>(std::move(state)), headerOptions);
    addWeighted("body", body);
}

