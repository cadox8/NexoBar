#include "SettingsWorkspaceScreen.h"

#include "../../../NexoBar.h"
#include "../../../printer/PrinterDiagnostics.h"
#include "../../../printer/TicketPrinter.h"
#include "../../../template/TicketTemplate.h"
#include "database/hospitality/Money.h"
#include "database/models/Business.h"
#include "database/models/CashSession.h"
#include "database/models/Payment.h"
#include "database/models/RolePermission.h"
#include "database/models/Ticket.h"
#include "database/models/TicketLine.h"
#include "security/Pin.h"
#include "license/License.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <ranges>
#include <sstream>

namespace {
    std::string kindTitleKey(const SettingsWorkspaceScreen::Kind kind) {
        switch (kind) {
            case SettingsWorkspaceScreen::Kind::Users:
                return "settings.users_title";
            case SettingsWorkspaceScreen::Kind::Roles:
                return "settings.roles_title";
            case SettingsWorkspaceScreen::Kind::Permissions:
                return "settings.permissions";
            case SettingsWorkspaceScreen::Kind::Business:
                return "settings.business";
            case SettingsWorkspaceScreen::Kind::Template:
                return "settings.template";
            case SettingsWorkspaceScreen::Kind::Status:
                return "settings.status_title";
            case SettingsWorkspaceScreen::Kind::History:
                return "settings.history_title";
        }
        return "settings.title";
    }

    template<typename Id>
    std::string idText(const Id& id) {
        return std::to_string(id.get());
    }

    template<typename Id>
    Id selectedId(const DropdownComponentPtr& dropdown) {
        const auto* option = dropdown ? dropdown->selected() : nullptr;
        return Id{option ? std::stoll(option->value) : 0};
    }

    std::filesystem::path activeTemplatePath() {
        const auto installed = std::filesystem::path(GetApplicationDirectory()) /
                               "resources" / "tickets" / "ticket.tpl";
        if (std::filesystem::is_regular_file(installed))
            return installed;
        return std::filesystem::current_path() / "NexoBar" / "resources" / "tickets" /
               "ticket.tpl";
    }

    std::string_view licenseTypeKey(const License::LicenseType type) {
        switch (type) {
            case License::LicenseType::Basic: return "license_types.basic";
            case License::LicenseType::Standard: return "license_types.standard";
            case License::LicenseType::Pro: return "license_types.professional";
            case License::LicenseType::Enterprise: return "license_types.enterprise";
            case License::LicenseType::Invalid: return "license_types.invalid";
        }
        return "license_types.invalid";
    }
} // namespace

SettingsWorkspaceScreen::SettingsWorkspaceScreen(const Kind kind,
                                                 std::function<void()> back)
    : m_kind(kind), m_back(std::move(back)),
      m_root(std::make_shared<ScrollableBlock>(LayoutAxis::Vertical,
                                               Color{27, 34, 45, 255})),
      m_title(std::make_shared<StaticText>()),
      m_summary(std::make_shared<StaticText>()),
      m_status(std::make_shared<StaticText>()),
      m_historyDate(boost::gregorian::day_clock::local_day()) {
    NexoBarImp app;
    m_font = app->defaultFont();
    m_virtualKeyboard = std::make_shared<VirtualKeyboard>([this] {
        const auto found = std::ranges::find_if(m_fields, [](const auto& field) { return field->focused; });
        return found == m_fields.end() ? InputTextPtr{} : *found;
    });
    m_virtualKeyboard->font = m_font;
    for (const auto& text: {m_title, m_summary, m_status}) {
        text->font = m_font;
        text->wrap = true;
        text->color = RAYWHITE;
    }
    m_summary->color = Color{190, 199, 214, 255};
    m_status->color = Color{126, 225, 185, 255};
    m_title->text = app->text(kindTitleKey(kind));
    addActions({}, {
                   {
                           app->text("settings.back"), [this] {
                           if (m_back)
                               m_back();
                       }
                   }
               });
    m_root->content.add(m_title, LayoutSize::fill(), LayoutSize::px(52));
    switch (kind) {
        case Kind::Users:
        case Kind::Roles:
        case Kind::Permissions:
            buildStaff();
            break;
        case Kind::Business:
            buildBusiness();
            break;
        case Kind::Template:
            buildTemplate();
            break;
        case Kind::Status:
            buildStatus();
            break;
        case Kind::History:
            buildHistory();
            break;
    }
    m_root->content.add(m_status, LayoutSize::fill(), LayoutSize::px(58));
}

StaticTextPtr SettingsWorkspaceScreen::addLabel(std::string text,
                                                const float height) {
    auto label = std::make_shared<StaticText>(std::move(text));
    label->font = m_font;
    label->color = Color{213, 220, 231, 255};
    label->verticalAlignment = TextVerticalAlignment::Bottom;
    m_labels.push_back(label);
    m_root->content.add(label, LayoutSize::fill(), LayoutSize::px(height));
    return label;
}

InputTextPtr SettingsWorkspaceScreen::addField(std::string placeholder,
                                               std::string value,
                                               const bool multiline,
                                               const float height) {
    auto field = std::make_shared<InputText>();
    field->font = m_font;
    field->placeholder = std::move(placeholder);
    field->multiline = multiline;
    field->maxLength = multiline ? 30000 : 256;
    field->setText(std::move(value));
    m_fields.push_back(field);
    m_root->content.add(field, LayoutSize::fill(), LayoutSize::px(height));
    return field;
}

DropdownComponentPtr SettingsWorkspaceScreen::addDropdown(const float height) {
    auto dropdown = std::make_shared<DropdownComponent>();
    dropdown->font = m_font;
    dropdown->optionHeight = 40;
    m_dropdowns.push_back(dropdown);
    m_root->content.add(dropdown, LayoutSize::fill(), LayoutSize::px(height));
    return dropdown;
}

SettingsActionRowPtr SettingsWorkspaceScreen::addActions(
    std::string label, std::vector<SettingsActionRow::Action> actions,
    const float height) {
    auto row = std::make_shared<SettingsActionRow>(std::move(label), m_font,
                                                   std::move(actions));
    m_actionRows.push_back(row);
    m_root->content.add(row, LayoutSize::fill(), LayoutSize::px(height));
    return row;
}

void SettingsWorkspaceScreen::setStatus(std::string text, const bool error) {
    m_status->text = std::move(text);
    m_status->color =
            error ? Color{255, 135, 125, 255} : Color{126, 225, 185, 255};
}

void SettingsWorkspaceScreen::buildStaff() {
    NexoBarImp app;
    m_root->content.add(m_summary, LayoutSize::fill(), LayoutSize::px(52));
    if (m_kind == Kind::Users) {
        addLabel(app->text("settings.employee"));
        m_userSelect = addDropdown();
        m_userSelect->onSelectionChanged = [this](int, const DropdownOption&) {
            loadUser();
        };
        m_userName = addField(app->text("settings.employee_name"));
        addLabel(app->text("settings.employee_pin"));
        m_userPin = addField(app->text("settings.employee_pin_placeholder"));
        m_userPin->password = true;
        m_userPin->maxLength = 6;
        addLabel(app->text("settings.employee_role"));
        m_userRoleSelect = addDropdown();
        addActions({}, {
                       {
                           app->text("settings.new"),
                           [this] {
                               m_userSelect->selectedIndex = -1;
                               m_userName->setText({});
                               m_userPin->setText({});
                           }
                       },
                       {app->text("settings.save"), [this] { saveUser(); }},
                       {app->text("settings.toggle"), [this] { toggleUser(); }},
                       {app->text("settings.delete"), [this] { removeUser(); }, true}
                   });
    } else if (m_kind == Kind::Roles) {
        addLabel(app->text("settings.role"));
        m_roleSelect = addDropdown();
        m_roleSelect->onSelectionChanged = [this](int, const DropdownOption&) {
            loadRole();
        };
        m_roleName = addField(app->text("settings.role_name"));
        m_roleDescription = addField(app->text("settings.role_description"));
        addActions({}, {
                       {
                           app->text("settings.new"),
                           [this] {
                               m_roleSelect->selectedIndex = -1;
                               m_roleName->setText({});
                               m_roleDescription->setText({});
                           }
                       },
                       {app->text("settings.save"), [this] { saveRole(); }},
                       {app->text("settings.toggle"), [this] { toggleRole(); }},
                       {app->text("settings.delete"), [this] { removeRole(); }, true}
                   });
    } else {
        addLabel(app->text("settings.role"));
        m_roleSelect = addDropdown();
        m_roleSelect->onSelectionChanged = [this](int, const DropdownOption&) {
            showPermissionState();
        };
        addLabel(app->text("settings.permission_existing"));
        m_permissionSelect = addDropdown();
        m_permissionSelect->onSelectionChanged =
                [this](int, const DropdownOption&) { showPermissionState(); };
        addActions(app->text("settings.assignment"),
                   {
                       {app->text("settings.assign"), [this] { updatePermission(true); }},
                       {app->text("settings.unassign"), [this] { updatePermission(false); }}
                   });
    }
    refreshStaff();
}

void SettingsWorkspaceScreen::refreshStaff() {
    NexoBarImp app;
    try {
        m_users = app->database()
                ->query<nexobar::User>()
                .orderBy(Nearon::col < &nexobar::User::displayName >.asc())
                .all();
        m_roles = app->database()
                ->query<nexobar::Role>()
                .orderBy(Nearon::col < &nexobar::Role::name >.asc())
                .all();
        m_permissions =
                app->database()
                ->query<nexobar::Permission>()
                .where(Nearon::col < &nexobar::Permission::active > == true)
                .orderBy(Nearon::col < &nexobar::Permission::key >.asc())
                .all();
        std::vector<DropdownOption> users, roles, permissions;
        for (const auto& value: m_users)
            users.push_back({
                value.displayName + (value.active ? "" : app->text("settings.inactive_suffix")),
                idText(value.id)
            });
        for (const auto& value: m_roles)
            roles.push_back(
                {value.name + (value.active ? "" : app->text("settings.inactive_suffix")), idText(value.id)});
        for (const auto& value: m_permissions)
            permissions.push_back(
                {app->text("permissions." + value.key), idText(value.id)});
        if (m_userSelect)
            m_userSelect->setOptions(std::move(users));
        if (m_userRoleSelect)
            m_userRoleSelect->setOptions(roles);
        if (m_roleSelect)
            m_roleSelect->setOptions(std::move(roles));
        if (m_permissionSelect)
            m_permissionSelect->setOptions(std::move(permissions));
        m_summary->text = app->text("settings.staff_summary");
        replaceToken(m_summary->text, "{users}", std::to_string(m_users.size()));
        replaceToken(m_summary->text, "{roles}", std::to_string(m_roles.size()));
        replaceToken(m_summary->text, "{permissions}", std::to_string(m_permissions.size()));
        if (m_userSelect)
            loadUser();
        if (m_roleSelect&& m_kind == Kind::Roles)
        loadRole();
        if (m_permissionSelect)
            showPermissionState();
    } catch (const std::exception& error) {
        setStatus(app->userError(error), true);
    }
}

void SettingsWorkspaceScreen::loadUser() {
    if (!m_userSelect || !m_userSelect->selected())
        return;
    const auto found = std::ranges::find(
        m_users, selectedId<nexobar::UserId>(m_userSelect), &nexobar::User::id);
    if (found == m_users.end())
        return;
    m_userName->setText(found->displayName);
    m_userPin->setText({});
    for (std::size_t i = 0; i < m_userRoleSelect->options.size(); ++i)
        if (m_userRoleSelect->options[i].value == idText(found->roleId))
            m_userRoleSelect->select(static_cast<int>(i));
}

void SettingsWorkspaceScreen::loadRole() {
    if (!m_roleSelect || !m_roleSelect->selected())
        return;
    const auto found = std::ranges::find(
        m_roles, selectedId<nexobar::RoleId>(m_roleSelect), &nexobar::Role::id);
    if (found == m_roles.end())
        return;
    m_roleName->setText(found->name);
    m_roleDescription->setText(found->description);
}

void SettingsWorkspaceScreen::saveUser() {
    NexoBarImp app;
    try {
        if (m_userName->text.empty())
            throw nexobar::HospitalityError("settings.employee_name_required");
        if (!m_userPin->text.empty() && !nexobar::Pin::valid(m_userPin->text))
            throw nexobar::HospitalityError("settings.employee_pin_invalid");
        const auto roleId = selectedId<nexobar::RoleId>(m_userRoleSelect);
        if (!roleId.get())
            throw nexobar::HospitalityError("settings.role_required");
        if (m_userSelect->selected()) {
            auto value = app->database()->get<nexobar::User>(
                selectedId<nexobar::UserId>(m_userSelect));
            value.displayName = m_userName->text;
            value.roleId = roleId;
            if (!m_userPin->text.empty()) {
                value.pinSalt = nexobar::Pin::newSalt();
                value.pinHash = nexobar::Pin::hash(m_userPin->text, value.pinSalt);
            }
            app->database()->update(value);
        } else {
            nexobar::User value;
            value.displayName = m_userName->text;
            value.roleId = roleId;
            value.createdAt = boost::posix_time::second_clock::local_time();
            if (!m_userPin->text.empty()) {
                value.pinSalt = nexobar::Pin::newSalt();
                value.pinHash = nexobar::Pin::hash(m_userPin->text, value.pinSalt);
            }
            app->database()->insert(value);
        }
        setStatus(app->text("settings.employee_saved"));
        refreshStaff();
    } catch (const std::exception& error) {
        setStatus(app->userError(error), true);
    }
}

void SettingsWorkspaceScreen::saveRole() {
    NexoBarImp app;
    try {
        if (m_roleName->text.empty())
            throw nexobar::HospitalityError("settings.role_name_required");
        if (m_roleSelect->selected()) {
            auto value = app->database()->get<nexobar::Role>(
                selectedId<nexobar::RoleId>(m_roleSelect));
            value.name = m_roleName->text;
            value.description = m_roleDescription->text;
            app->database()->update(value);
        } else {
            nexobar::Role value;
            value.name = m_roleName->text;
            value.description = m_roleDescription->text;
            app->database()->insert(value);
        }
        setStatus(app->text("settings.role_saved"));
        refreshStaff();
    } catch (const std::exception& error) {
        setStatus(app->userError(error), true);
    }
}

void SettingsWorkspaceScreen::toggleUser() {
    NexoBarImp app;
    try {
        if (!m_userSelect->selected())
            throw nexobar::HospitalityError("settings.employee_required");
        auto value = app->database()->get<nexobar::User>(
            selectedId<nexobar::UserId>(m_userSelect));
        value.active = !value.active;
        app->database()->update(value);
        setStatus(app->text(value.active ? "settings.employee_enabled" : "settings.employee_disabled"));
        refreshStaff();
    } catch (const std::exception& e) {
        setStatus(app->userError(e), true);
    }
}

void SettingsWorkspaceScreen::toggleRole() {
    NexoBarImp app;
    try {
        if (!m_roleSelect->selected())
            throw nexobar::HospitalityError("settings.role_required");
        auto value = app->database()->get<nexobar::Role>(
            selectedId<nexobar::RoleId>(m_roleSelect));
        value.active = !value.active;
        app->database()->update(value);
        setStatus(app->text(value.active ? "settings.role_enabled" : "settings.role_disabled"));
        refreshStaff();
    } catch (const std::exception& e) {
        setStatus(app->userError(e), true);
    }
}

void SettingsWorkspaceScreen::removeUser() {
    NexoBarImp app;
    try {
        if (!m_userSelect->selected())
            throw nexobar::HospitalityError("settings.employee_required");
        app->database()->remove<nexobar::User>(
            selectedId<nexobar::UserId>(m_userSelect));
        setStatus(app->text("settings.employee_deleted"));
        refreshStaff();
    } catch (const std::exception& e) {
        setStatus(app->userError(e, "settings.delete_failed"), true);
    }
}

void SettingsWorkspaceScreen::removeRole() {
    NexoBarImp app;
    try {
        if (!m_roleSelect->selected())
            throw nexobar::HospitalityError("settings.role_required");
        app->database()->remove<nexobar::Role>(
            selectedId<nexobar::RoleId>(m_roleSelect));
        setStatus(app->text("settings.role_deleted"));
        refreshStaff();
    } catch (const std::exception& e) {
        setStatus(app->userError(e, "settings.delete_failed"), true);
    }
}

void SettingsWorkspaceScreen::showPermissionState() {
    if (!m_roleSelect || !m_permissionSelect || !m_roleSelect->selected() ||
        !m_permissionSelect->selected())
        return;
    NexoBarImp app;
    const auto roleId = selectedId<nexobar::RoleId>(m_roleSelect);
    const auto permissionId =
            selectedId<nexobar::PermissionId>(m_permissionSelect);
    const bool assigned =
            app->database()->count<nexobar::RolePermission>(
                Nearon::col < &nexobar::RolePermission::roleId > == roleId &&
                Nearon::col < &nexobar::RolePermission::permissionId > == permissionId) >
            0;
    setStatus(app->text(assigned ? "settings.permission_assigned" : "settings.permission_unassigned"));
}

void SettingsWorkspaceScreen::updatePermission(const bool assign) {
    NexoBarImp app;
    try {
        if (!m_roleSelect->selected() || !m_permissionSelect->selected())
            throw nexobar::HospitalityError("settings.role_permission_required");
        const auto roleId = selectedId<nexobar::RoleId>(m_roleSelect);
        const auto permissionId =
                selectedId<nexobar::PermissionId>(m_permissionSelect);
        const auto condition =
                Nearon::col < &nexobar::RolePermission::roleId > == roleId &&
                Nearon::col < &nexobar::RolePermission::permissionId > == permissionId;
        if (assign &&
            app->database()->count<nexobar::RolePermission>(condition) == 0) {
            nexobar::RolePermission link;
            link.roleId = roleId;
            link.permissionId = permissionId;
            app->database()->insert(link);
        }
        if (!assign)
            app->database()->removeWhere<nexobar::RolePermission>(condition);
        showPermissionState();
    } catch (const std::exception& error) {
        setStatus(app->userError(error), true);
    }
}

void SettingsWorkspaceScreen::buildBusiness() {
    NexoBarImp app;
    const auto& business = app->business();
    const std::vector<std::pair<std::string, std::string> > values = {
        {app->text("settings.field_legal_name"), business.legalName},
        {app->text("settings.field_trade_name"), business.tradeName},
        {app->text("settings.field_nif"), business.nif},
        {app->text("settings.field_address"), business.address},
        {app->text("settings.field_postal_code"), business.postalCode},
        {app->text("settings.field_city"), business.city},
        {app->text("settings.field_province"), business.province},
        {app->text("settings.field_country"), business.country},
        {app->text("settings.field_phone"), business.phone.value_or("")},
        {app->text("settings.field_email"), business.email.value_or("")},
        {app->text("settings.field_license"), business.license}
    };
    for (const auto& [label, value]: values) {
        addLabel(label);
        addField(label, value);
    }
    addActions({}, {{app->text("settings.save_configuration"), [this] { saveBusiness(); }}});
}

void SettingsWorkspaceScreen::saveBusiness() {
    NexoBarImp app;
    try {
        if (m_fields.size() != 11)
            throw nexobar::HospitalityError("settings.business_form_incomplete");
        auto value = app->business();
        value.legalName = m_fields[0]->text;
        value.tradeName = m_fields[1]->text;
        value.nif = m_fields[2]->text;
        value.address = m_fields[3]->text;
        value.postalCode = m_fields[4]->text;
        value.city = m_fields[5]->text;
        value.province = m_fields[6]->text;
        value.country = m_fields[7]->text;
        value.phone = m_fields[8]->text.empty()
                          ? std::nullopt
                          : std::optional<std::string>{m_fields[8]->text};
        value.email = m_fields[9]->text.empty()
                          ? std::nullopt
                          : std::optional<std::string>{m_fields[9]->text};
        value.license = m_fields[10]->text;
        if (!app->database()->update(value))
            value.id = app->database()->insert(value);
        app->setBusiness(value);
        setStatus(app->text("settings.business_saved"));
    } catch (const std::exception& error) {
        setStatus(app->userError(error), true);
    }
}

void SettingsWorkspaceScreen::buildTemplate() {
    NexoBarImp app;
    m_templatePath = activeTemplatePath().string();
    addLabel(app->text("settings.template_blocks"), 44.f);
    m_templateBlocks = std::make_shared<TicketTemplateBlockEditor>();
    m_templateBlocks->font = m_font;
    m_root->content.add(m_templateBlocks, LayoutSize::fill(), LayoutSize::px(560));
    std::ifstream input(m_templatePath, std::ios::binary);
    std::ostringstream data;
    data << input.rdbuf();
    m_templateBlocks->load(data.str());
    addActions({}, {
                   {app->text("settings.save"), [this] { saveTemplate(); }},
                   {app->text("settings.reload"), [this] { reloadTemplate(); }},
                   {app->text("settings.print_test"), [this] { printTemplateTest(false); }},
                   {app->text("settings.test_and_drawer"), [this] { printTemplateTest(true); }}
               });
}

void SettingsWorkspaceScreen::saveTemplate() {
    NexoBarImp app;
    try {
        std::ofstream output(m_templatePath, std::ios::binary | std::ios::trunc);
        if (!output)
            throw nexobar::HospitalityError("settings.template_write_open_failed");
        output << m_templateBlocks->templateText();
        if (!output)
            throw nexobar::HospitalityError("settings.template_save_failed");
        setStatus(app->text("settings.template_saved"));
    } catch (const std::exception& error) {
        setStatus(app->userError(error), true);
    }
}

void SettingsWorkspaceScreen::reloadTemplate() {
    NexoBarImp app;
    try {
        std::ifstream input(m_templatePath, std::ios::binary);
        if (!input)
            throw nexobar::HospitalityError("settings.template_open_failed");
        std::ostringstream data;
        data << input.rdbuf();
        m_templateBlocks->load(data.str());
        setStatus(app->text("settings.template_reloaded"));
    } catch (const std::exception& error) {
        setStatus(app->userError(error), true);
    }
}

void SettingsWorkspaceScreen::printTemplateTest(const bool drawer) {
    NexoBarImp app;
    try {
        nexobar::Ticket ticket;
        ticket.fullNumber = app->text("receipt.test_number");
        ticket.seriesCode = app->text("receipt.test_series");
        ticket.number = 1;
        ticket.issuedAt = boost::posix_time::second_clock::local_time();
        ticket.issuerName = app->business().tradeName;
        ticket.issuerNif = app->business().nif;
        ticket.issuerAddress = app->business().address;
        ticket.taxBase = 826;
        ticket.taxAmount = 174;
        ticket.total = 1000;
        nexobar::TicketLine line;
        line.lineNumber = 1;
        line.description = app->text("settings.test_line");
        line.quantity = 1000;
        line.unitPrice = 1000;
        line.total = 1000;
        const auto rendered = TicketTemplate::renderFile(
            m_templatePath, ticket, {line}, app->business().tradeName, *app->i18n(),
            {}, PrinterDiagnostics::detectedReceiptLayout());
        TicketPrinter printer;
        std::string error;
        const bool ok =
                drawer
                    ? printer.printTicketAndOpenDrawer(rendered, error, *app->i18n())
                    : printer.printText(rendered, error, *app->i18n());
        setStatus(ok ? app->text("settings.test_sent") : error, !ok);
    } catch (const std::exception& error) {
        setStatus(app->userError(error), true);
    }
}

void SettingsWorkspaceScreen::buildStatus() {
    NexoBarImp app;
    m_templatePath = activeTemplatePath().string();
    m_root->content.add(m_summary, LayoutSize::fill(), LayoutSize::px(205));
    addActions(app->text("settings.device"),
               {{app->text("settings.refresh"), [this] { refreshDeviceStatus(); }}});
    addActions(app->text("settings.tests"),
               {
                   {app->text("settings.ticket"), [this] { printTemplateTest(false); }},
                   {
                       app->text("settings.drawer"),
                       [this] {
                           NexoBarImp app;
                           std::string error;
                           const bool ok =
                                   PrinterDiagnostics::testDrawer(error, *app->i18n());
                           setStatus(ok ? app->text("settings.drawer_opened") : error, !ok);
                       }
                   },
                   {app->text("settings.ticket_and_drawer"), [this] { printTemplateTest(true); }}
               });
    refreshDeviceStatus();
}

void SettingsWorkspaceScreen::refreshDeviceStatus() {
    NexoBarImp app;
    License license;
    std::ostringstream output;
    output << app->text("settings.license_prefix")
            << app->text(licenseTypeKey(license.validate(app->business().license))) << "\n";
    std::string error;
    const auto printers = PrinterDiagnostics::enumerateUsbPrinters(error, app->i18n());
    if (!error.empty())
        output << app->text("settings.printer_detection_error") << " (" << error << ")\n";
    else if (printers.empty())
        output << app->text("settings.printer_none") << "\n";
    for (const auto& printer: printers) {
        std::ostringstream usbIdentifier;
        usbIdentifier << std::hex << std::setw(4) << std::setfill('0') << printer.vendorId
                      << ':' << std::setw(4) << printer.productId;
        std::string usbLocation = app->text("settings.usb_location");
        replaceToken(usbLocation, "{identifier}", usbIdentifier.str());
        replaceToken(usbLocation, "{bus}", std::to_string(printer.bus));
        replaceToken(usbLocation, "{device}", std::to_string(printer.address));
        output << app->text("settings.printer_prefix") << printer.name
                << "\n" << usbLocation
                << "\n" << app->text("settings.manufacturer")
                << (printer.manufacturer.empty() ? app->text("settings.not_reported") : printer.manufacturer)
                << "\n" << app->text("settings.paper");
        if (printer.paperWidthMm)
            output << printer.paperWidthMm << app->text("units.millimeters_suffix");
        else
            output << app->text("settings.paper_not_reported");
        if (printer.paperHeightMm)
            output << app->text("units.dimension_separator") << printer.paperHeightMm
                   << app->text("units.millimeters_suffix");
        output << "\n" << app->text("settings.commands") << printer.commandSet;
        if (printer.dpi)
            output << app->text("units.detail_separator") << printer.dpi
                   << app->text("units.dpi_suffix");
        output << "\n";
    }
    m_summary->text = output.str();
    setStatus(app->text("settings.state_updated"));
}

void SettingsWorkspaceScreen::buildHistory() {
    NexoBarImp app;
    m_root->content.add(m_summary, LayoutSize::fill(), LayoutSize::px(235));
    addActions(app->text("settings.period"), {
                   {
                       app->text("settings.day"),
                       [this] {
                           m_historyPeriod = HistoryPeriod::Day;
                           refreshHistory();
                       }
                   },
                   {
                       app->text("settings.month"),
                       [this] {
                           m_historyPeriod = HistoryPeriod::Month;
                           refreshHistory();
                       }
                   },
                   {
                       app->text("settings.all"), [this] {
                           m_historyPeriod = HistoryPeriod::All;
                           refreshHistory();
                       }
                   }
               });
    addActions(app->text("settings.date"), {
                   {app->text("settings.previous"), [this] { moveHistory(-1); }},
                   {
                       app->text("settings.today"),
                       [this] {
                           m_historyDate =
                                   boost::gregorian::day_clock::local_day();
                           refreshHistory();
                       }
                   },
                   {app->text("settings.next"), [this] { moveHistory(1); }}
               });
    addActions(app->text("settings.report"),
               {
                   {app->text("settings.print"), [this] { printHistory(false); }},
                   {app->text("settings.close_day"), [this] { printHistory(true); }}
               });
    refreshHistory();
}

void SettingsWorkspaceScreen::moveHistory(const int amount) {
    if (m_historyPeriod == HistoryPeriod::Day)
        m_historyDate += boost::gregorian::days(amount);
    else if (m_historyPeriod == HistoryPeriod::Month)
        m_historyDate += boost::gregorian::months(amount);
    refreshHistory();
}

std::string SettingsWorkspaceScreen::historyReport() const {
    NexoBarImp app;
    const auto tickets = app->database()->selectAll<nexobar::Ticket>();
    const auto payments = app->database()->selectAll<nexobar::Payment>();
    const auto included = [this](const boost::posix_time::ptime& time) {
        if (m_historyPeriod == HistoryPeriod::All)
            return true;
        if (m_historyPeriod == HistoryPeriod::Day)
            return time.date() == m_historyDate;
        return time.date().year() == m_historyDate.year() &&
               time.date().month() == m_historyDate.month();
    };
    std::int64_t base = 0, tax = 0, total = 0, cash = 0, card = 0;
    std::size_t count = 0;
    std::ostringstream detail;
    for (const auto& ticket: tickets)
        if (included(ticket.issuedAt)) {
            ++count;
            base += ticket.taxBase;
            tax += ticket.taxAmount;
            total += ticket.total;
            detail << ticket.fullNumber << "  "
                    << boost::posix_time::to_simple_string(ticket.issuedAt) << "  "
                    << nexobar::formatMoney(ticket.total, *app->i18n()) << "\n";
        }
    for (const auto& payment: payments)
        if (included(payment.createdAt))
            (payment.method == nexobar::PaymentMethod::Cash ? cash : card) +=
                    payment.amount;
    std::ostringstream output;
    output << app->text("settings.report_header") << "\n";
    if (m_historyPeriod == HistoryPeriod::Day)
        output << app->text("settings.report_day") << boost::gregorian::to_iso_extended_string(m_historyDate)
                << "\n";
    else if (m_historyPeriod == HistoryPeriod::Month)
        output << app->text("settings.report_month") << m_historyDate.year() << "-" << std::setw(2)
                << std::setfill('0') << static_cast<int>(m_historyDate.month())
                << std::setfill(' ') << "\n";
    else
        output << app->text("settings.report_all") << "\n";
    output << app->text("settings.report_tickets") << count
            << "\n" << app->text("settings.report_tax_base") << nexobar::formatMoney(base, *app->i18n())
            << "\n" << app->text("settings.report_tax") << nexobar::formatMoney(tax, *app->i18n())
            << "\n" << app->text("settings.report_total") << nexobar::formatMoney(total, *app->i18n())
            << "\n" << app->text("settings.report_cash") << nexobar::formatMoney(cash, *app->i18n())
            << "\n" << app->text("settings.report_card") << nexobar::formatMoney(card, *app->i18n())
            << "\n" << app->text("reports.separator") << "\n"
            << detail.str();
    return output.str();
}

void SettingsWorkspaceScreen::refreshHistory() {
    try {
        m_summary->text = historyReport();
        NexoBarImp app;
        setStatus(app->text("settings.history_updated"));
    } catch (const std::exception& error) {
        NexoBarImp app;
        setStatus(app->userError(error), true);
    }
}

void SettingsWorkspaceScreen::printHistory(const bool closeDay) {
    NexoBarImp app;
    try {
        if (closeDay && m_historyPeriod != HistoryPeriod::Day)
            throw nexobar::HospitalityError("settings.close_day_required");
        TicketPrinter printer;
        std::string error;
        if (!printer.printText(historyReport(), error, *app->i18n())) {
            setStatus(error.empty() ? app->text("errors.operation_failed") : error, true);
            return;
        }
        std::size_t closed = 0;
        if (closeDay) {
            const auto payments = app->database()->selectAll<nexobar::Payment>();
            auto sessions = app->database()->selectAll<nexobar::CashSession>();
            const auto users = app->database()->selectAll<nexobar::User>();
            for (auto& session: sessions)
                if (!session.closedAt && session.openedAt.date() == m_historyDate) {
                    std::int64_t cash = 0;
                    for (const auto& payment: payments)
                        if (payment.cashSessionId == session.id &&
                            payment.method == nexobar::PaymentMethod::Cash)
                            cash += payment.amount;
                    session.closedAt = boost::posix_time::second_clock::local_time();
                    session.expectedAmount = session.openingAmount + cash;
                    if (!users.empty())
                        session.closedBy = users.front().id;
                    app->database()->update(session);
                    ++closed;
                }
        }
        setStatus(closeDay
                      ? app->text("settings.close_printed") + std::to_string(closed)
                      : app->text("settings.report_printed"));
    } catch (const std::exception& error) {
        setStatus(app->userError(error), true);
    }
}

void SettingsWorkspaceScreen::layout() {
    const float unit = std::max(1.f, std::min(m_area.width, m_area.height));
    const float inset = std::clamp(unit * .03f, 12.f, 25.f);
    m_root->content.padding(LayoutInsets::all(inset))
            .gap(std::clamp(unit * .012f, 6.f, 11.f));
    m_root->area(m_area);
    m_title->fontSize = std::clamp(unit * .055f, 22.f, 35.f);
    m_summary->fontSize = std::clamp(unit * .027f, 14.f, 18.f);
    m_status->fontSize = m_summary->fontSize;
    for (const auto& item: m_labels)
        item->fontSize = m_summary->fontSize;
    const int bodyFontSize = static_cast<int>(m_summary->fontSize);
    for (const auto& item: m_fields)
        item->fontSize = bodyFontSize;
    for (const auto& item: m_dropdowns)
        item->fontSize = bodyFontSize;
    for (const auto& item: m_actionRows)
        item->setFontSize(bodyFontSize);
}

void SettingsWorkspaceScreen::update(const float deltaSeconds) {
    layout();
    m_virtualKeyboard->area({m_area.x + 12.f, m_area.y + m_area.height - 190.f,
                             std::max(0.f, m_area.width - 24.f), 178.f});
    m_virtualKeyboard->update(deltaSeconds);
    if (m_virtualKeyboard->visible) UIInput::suppressClicks();
    m_root->update(deltaSeconds);
}

void SettingsWorkspaceScreen::draw() {
    layout();
    m_root->draw();
    for (const auto& dropdown: m_dropdowns)
        dropdown->drawOverlay();
    if (m_virtualKeyboard->visible) {
        DrawRectangleRec({m_area.x, m_area.y + m_area.height - 205.f, m_area.width, 205.f},
                         Color{18, 26, 37, 245});
        m_virtualKeyboard->draw();
    }
}
