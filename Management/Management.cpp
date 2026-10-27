#include "Management.h"

#include "menus/LicenseMenu.h"
#include "menus/MainMenu.h"
#include "menus/BusinessMenu.h"
#include "menus/SimulationMenu.h"
#include "database/models/Business.h"
#include "database/hospitality/HospitalityDatabase.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>

#include <algorithm>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <vector>

Management* ManagementImp::m_nexo = nullptr;

namespace {
bool sameBusiness(const nexobar::Business& left, const nexobar::Business& right) {
    return left.id == right.id && left.legalName == right.legalName &&
           left.tradeName == right.tradeName && left.nif == right.nif &&
           left.address == right.address && left.postalCode == right.postalCode &&
           left.city == right.city && left.province == right.province &&
           left.country == right.country && left.phone == right.phone &&
           left.email == right.email && left.license == right.license;
}

const std::unordered_map<std::string_view, std::string_view> managementTexts{
    {"permissions.sell", "Add products and manual amounts to a list"},
    {"permissions.discount", "Apply discounts to consumption lines"},
    {"permissions.remove_line", "Remove lines from an open list"},
    {"permissions.close_list", "Take payment and close consumption lists"},
    {"permissions.print_ticket", "Print the receipt for a closed sale"},
    {"permissions.open_drawer", "Open the cash drawer connected to the printer"},
    {"permissions.void_ticket", "Request a ticket cancellation or correction"},
    {"permissions.manage_products", "Manage categories, taxes, and products"},
    {"permissions.manage_users", "Manage users, roles, and permissions"},
    {"permissions.manage_roles", "Create, edit, enable, and disable roles"},
    {"permissions.manage_permissions", "Grant or revoke permissions for each role"},
    {"permissions.manage_cash", "Open, close, and reconcile the cash register"},
    {"permissions.view_reports", "View sales and cash reports"},
    {"permissions.print_reports", "Print sales reports for a selected period"},
    {"permissions.configure_business", "Configure business and invoicing details"},
    {"permissions.configure_printer", "Configure and test the USB printer and cash drawer"},
    {"permissions.edit_ticket_template", "Edit receipt template blocks and placeholders"},
    {"permissions.export_fiscal", "Export and verify fiscal records"},
    {"roles.administrator.name", "Administrator"},
    {"roles.administrator.description", "Full access"},
    {"roles.manager.name", "Manager"},
    {"roles.manager.description", "Manage shifts and the product catalogue"},
    {"roles.bartender.name", "Bartender"},
    {"roles.bartender.description", "Sell and take payment for lists"},
    {"users.default_admin_name", "Administrator"},
    {"taxes.general_21", "Standard VAT 21%"},
    {"taxes.reduced_10", "Reduced VAT 10%"},
    {"taxes.super_reduced_4", "Super-reduced VAT 4%"},
    {"taxes.exempt_0", "Exempt 0%"},
    {"categories.default", "Default"},
    {"categories.drinks", "Drinks"},
    {"categories.coffee", "Coffee"},
    {"categories.other", "Other"},
    {"ticket_series.simplified", "Simplified tickets"},
    {"lists.manual_amount", "Manual"},
};
}

void Management::initializeDefaults(const std::filesystem::path& databasePath) {
    std::vector<nexobar::Business> businessesBefore;
    if (std::filesystem::exists(databasePath) && std::filesystem::file_size(databasePath) > 0) {
        Nearon::Database existing(databasePath);
        const auto hasBusinessTable = existing.scalar<std::int64_t>(
            "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='business'");
        if (hasBusinessTable > 0)
            businessesBefore = existing.selectAll<nexobar::Business>();
    }

    m_database = nexobar::openHospitalityDatabase(
        databasePath, [this](const std::string_view key) { return text(key); });
    if (!businessesBefore.empty()) {
        const auto businessesAfter = m_database->selectAll<nexobar::Business>();
        if (businessesBefore.size() != businessesAfter.size())
            throw std::runtime_error("Business maintenance was cancelled because existing business data changed.");
        for (const auto& before: businessesBefore) {
            const auto after = std::ranges::find(businessesAfter, before.id,
                                                  &nexobar::Business::id);
            if (after == businessesAfter.end() || !sameBusiness(before, *after))
                throw std::runtime_error("Business maintenance was cancelled because existing business data changed.");
        }
    }
}

void Management::init() {
    initializeDefaults("nexobar.db");
    m_backups = std::make_unique<Nearon::AutomaticBackup>(*m_database, m_database->path().parent_path() / "backups");
    m_backups->start();
    auto screen = ftxui::ScreenInteractive::Fullscreen();
    MainMenu mainMenu;
    LicenseMenu licenseMenu;
    BusinessMenu businessMenu(*m_database);
    SimulationMenu simulationMenu(*m_database);
    mainMenu.init();
    licenseMenu.init();
    businessMenu.init();
    simulationMenu.init();
    int activeMenu = 0;

    mainMenu.on_open_licenses = [&] { activeMenu = 1; };
    mainMenu.on_register_business = [&] { businessMenu.refresh(); activeMenu = 2; };
    mainMenu.on_simulate_business = [&] { activeMenu = 3; };
    mainMenu.on_exit = [&] { screen.Exit(); };
    licenseMenu.on_back = [&] { activeMenu = 0; };
    businessMenu.on_back = [&] { activeMenu = 0; };
    simulationMenu.on_back = [&] { activeMenu = 0; };

    auto menus = ftxui::Container::Tab({mainMenu.getComponent(), licenseMenu.getComponent(), businessMenu.getComponent(), simulationMenu.getComponent()}, &activeMenu);
    screen.Loop(menus);
    shutdown();
}

void Management::shutdown() { m_backups.reset(); m_database.reset(); }

std::string Management::text(const std::string_view key) const {
    if (const auto found = managementTexts.find(key); found != managementTexts.end())
        return std::string(found->second);
    throw std::runtime_error("Missing internal Management text: " + std::string(key));
}

std::string Management::userError(const std::exception& error) const {
    if (const auto* hospitality = dynamic_cast<const nexobar::HospitalityError*>(&error)) {
        const std::string& key = hospitality->translationKey();
        if (key == "errors.sale_empty") return "An empty sale cannot be issued.";
        if (key == "errors.active_user_missing") return "The active employee no longer exists.";
        if (key == "errors.ticket_series_inactive") return "This year's ticket series is disabled.";
        if (key == "errors.sale_line_invalid") return "The sale contains an invalid line.";
        return "The operation could not be completed.";
    }
    const std::string detail = error.what();
    return detail.empty() ? "The operation could not be completed." : detail;
}
