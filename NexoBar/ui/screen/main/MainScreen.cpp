#include "MainScreen.h"

#include "../../../NexoBar.h"
#include "../menus/HeaderScreen.h"
#include "../reports/ReportScreen.h"
#include "../sales/ProductScreen.h"
#include "../settings/SettingsHomeScreen.h"
#include "../fiscal/TicketCancellationScreen.h"

MainScreen::MainScreen(std::shared_ptr<ProductState> state, std::function<void()> logout)
    : ScreenLayout(LayoutAxis::Vertical), m_content(std::make_shared<ScreenLayout>(LayoutAxis::Vertical)) {
    NexoBarImp nexo;
    auto sales = std::make_shared<ProductScreen>(state);
    auto reports = std::make_shared<ReportScreen>(nexobar::UserId{state->userId},
                                                  state->hasPermission("print_reports"), state->hasPermission("manage_cash"),
                                                  state->hasPermission("export_fiscal"));
    auto settings = std::make_shared<SettingsHomeScreen>(
        [state](const std::string& permission) { return state->hasPermission(permission); });
    m_content->addWeighted("page", sales);

    std::vector<SideMenuScreen::Item> items;
    items.push_back({
        .id = "sales", .label = nexo->text("menu.sales"), .icon = "icon-cart", .action = [this, sales] {
            m_content->setScreen("page", sales);
            m_menu->select("sales");
        }
    });
    items.push_back({
        .id = "cancellations", .label = nexo->text("menu.cancellations"), .icon = "icon-cancel", .action = [this, state] {
            m_content->setScreen("page", std::make_shared<TicketCancellationScreen>(nexobar::UserId{state->userId}));
            m_menu->select("cancellations");
        },
        .bottom = false, .enabled = state->hasPermission("void_ticket")
    });
    items.push_back({
        .id = "reports", .label = nexo->text("menu.reports"), .icon = "icon-reports", .action = [this, reports] {
            reports->activate();
            m_content->setScreen("page", reports);
            m_menu->select("reports");
        },
        .bottom = false, .enabled = state->hasPermission("view_reports")
    });
    items.push_back({
        .id = "settings", .label = nexo->text("menu.settings"), .icon = "icon-settings", .action = [this, settings] {
            m_content->setScreen("page", settings);
            m_menu->select("settings");
        },
        .bottom = true, .enabled = state->canOpenSettings()
    });
    items.push_back({.id = "logout", .label = nexo->text("menu.logout"), .icon = "icon-logout", .action = std::move(logout), .bottom = true, .enabled = true});
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
