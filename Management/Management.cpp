#include "Management.h"

#include "menus/LicenseMenu.h"
#include "menus/MainMenu.h"
#include "menus/BusinessMenu.h"
#include "database/models/Business.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>

Management* ManagementImp::m_nexo = nullptr;

void Management::init() {
    m_database = std::make_shared<Nearon::Database>("nexobar.db");
    m_database->createTables<nexobar::Business>();
    m_backups = std::make_unique<Nearon::AutomaticBackup>(*m_database, m_database->path().parent_path() / "backups");
    m_backups->start();
    auto screen = ftxui::ScreenInteractive::Fullscreen();
    MainMenu mainMenu;
    LicenseMenu licenseMenu;
    BusinessMenu businessMenu(*m_database);
    mainMenu.init();
    licenseMenu.init();
    businessMenu.init();
    int activeMenu = 0;

    mainMenu.on_open_licenses = [&] { activeMenu = 1; };
    mainMenu.on_register_business = [&] { businessMenu.refresh(); activeMenu = 2; };
    mainMenu.on_exit = [&] { screen.Exit(); };
    licenseMenu.on_back = [&] { activeMenu = 0; };
    businessMenu.on_back = [&] { activeMenu = 0; };

    auto menus = ftxui::Container::Tab({mainMenu.getComponent(), licenseMenu.getComponent(), businessMenu.getComponent()}, &activeMenu);
    screen.Loop(menus);
    shutdown();
}

void Management::shutdown() { m_backups.reset(); m_database.reset(); }
