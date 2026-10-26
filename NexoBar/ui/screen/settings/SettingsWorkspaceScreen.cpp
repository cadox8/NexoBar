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
#include "license/License.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <ranges>
#include <sstream>

namespace {
    std::string kindKey(const SettingsWorkspaceScreen::Kind kind) {
        switch (kind) {
            case SettingsWorkspaceScreen::Kind::Users:
                return "users";
            case SettingsWorkspaceScreen::Kind::Roles:
                return "roles";
            case SettingsWorkspaceScreen::Kind::Permissions:
                return "permissions";
            case SettingsWorkspaceScreen::Kind::Business:
                return "business";
            case SettingsWorkspaceScreen::Kind::Template:
                return "template";
            case SettingsWorkspaceScreen::Kind::Status:
                return "status";
            case SettingsWorkspaceScreen::Kind::History:
                return "history";
        }
        return "status";
    }

    std::string kindTitle(const SettingsWorkspaceScreen::Kind kind) {
        switch (kind) {
            case SettingsWorkspaceScreen::Kind::Users:
                return "Gestión de empleados";
            case SettingsWorkspaceScreen::Kind::Roles:
                return "Gestión de roles";
            case SettingsWorkspaceScreen::Kind::Permissions:
                return "Permisos por rol";
            case SettingsWorkspaceScreen::Kind::Business:
                return "Configuración del negocio";
            case SettingsWorkspaceScreen::Kind::Template:
                return "Plantilla del ticket";
            case SettingsWorkspaceScreen::Kind::Status:
                return "Información y estado";
            case SettingsWorkspaceScreen::Kind::History:
                return "Facturación y cierre";
        }
        return "Ajustes";
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
    for (const auto& text: {m_title, m_summary, m_status}) {
        text->font = m_font;
        text->wrap = true;
        text->color = RAYWHITE;
    }
    m_summary->color = Color{190, 199, 214, 255};
    m_status->color = Color{126, 225, 185, 255};
    m_title->text = app->text("settings." + kindKey(kind), kindTitle(kind));
    addActions({}, {
                   {
                       app->text("settings.back", "Volver"), [this] {
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
    m_root->content.add(m_summary, LayoutSize::fill(), LayoutSize::px(52));
    if (m_kind == Kind::Users) {
        addLabel("Empleado");
        m_userSelect = addDropdown();
        m_userSelect->onSelectionChanged = [this](int, const DropdownOption&) {
            loadUser();
        };
        m_userName = addField("Nombre del empleado");
        addLabel("Rol del empleado");
        m_userRoleSelect = addDropdown();
        addActions({}, {
                       {
                           "Nuevo",
                           [this] {
                               m_userSelect->selectedIndex = -1;
                               m_userName->setText({});
                           }
                       },
                       {"Guardar", [this] { saveUser(); }},
                       {"Activar / desactivar", [this] { toggleUser(); }},
                       {"Eliminar", [this] { removeUser(); }, true}
                   });
    } else if (m_kind == Kind::Roles) {
        addLabel("Rol");
        m_roleSelect = addDropdown();
        m_roleSelect->onSelectionChanged = [this](int, const DropdownOption&) {
            loadRole();
        };
        m_roleName = addField("Nombre del rol");
        m_roleDescription = addField("Descripción del rol");
        addActions({}, {
                       {
                           "Nuevo",
                           [this] {
                               m_roleSelect->selectedIndex = -1;
                               m_roleName->setText({});
                               m_roleDescription->setText({});
                           }
                       },
                       {"Guardar", [this] { saveRole(); }},
                       {"Activar / desactivar", [this] { toggleRole(); }},
                       {"Eliminar", [this] { removeRole(); }, true}
                   });
    } else {
        addLabel("Rol");
        m_roleSelect = addDropdown();
        m_roleSelect->onSelectionChanged = [this](int, const DropdownOption&) {
            showPermissionState();
        };
        addLabel("Permiso existente");
        m_permissionSelect = addDropdown();
        m_permissionSelect->onSelectionChanged =
                [this](int, const DropdownOption&) { showPermissionState(); };
        addActions("Asignación",
                   {
                       {"Asignar al rol", [this] { updatePermission(true); }},
                       {"Quitar del rol", [this] { updatePermission(false); }}
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
                value.displayName + (value.active ? "" : " (inactivo)"),
                idText(value.id)
            });
        for (const auto& value: m_roles)
            roles.push_back(
                {value.name + (value.active ? "" : " (inactivo)"), idText(value.id)});
        for (const auto& value: m_permissions)
            permissions.push_back(
                {value.key + " — " + value.description, idText(value.id)});
        if (m_userSelect)
            m_userSelect->setOptions(std::move(users));
        if (m_userRoleSelect)
            m_userRoleSelect->setOptions(roles);
        if (m_roleSelect)
            m_roleSelect->setOptions(std::move(roles));
        if (m_permissionSelect)
            m_permissionSelect->setOptions(std::move(permissions));
        m_summary->text = std::to_string(m_users.size()) + " empleados · " +
                          std::to_string(m_roles.size()) + " roles · " +
                          std::to_string(m_permissions.size()) +
                          " permisos del sistema";
        if (m_userSelect)
            loadUser();
        if (m_roleSelect&& m_kind == Kind::Roles)
        loadRole();
        if (m_permissionSelect)
            showPermissionState();
    } catch (const std::exception& error) {
        setStatus(error.what(), true);
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
            throw std::runtime_error("El nombre del empleado es obligatorio.");
        const auto roleId = selectedId<nexobar::RoleId>(m_userRoleSelect);
        if (!roleId.get())
            throw std::runtime_error("Selecciona un rol.");
        if (m_userSelect->selected()) {
            auto value = app->database()->get<nexobar::User>(
                selectedId<nexobar::UserId>(m_userSelect));
            value.displayName = m_userName->text;
            value.roleId = roleId;
            app->database()->update(value);
        } else {
            nexobar::User value;
            value.displayName = m_userName->text;
            value.roleId = roleId;
            value.createdAt = boost::posix_time::second_clock::local_time();
            app->database()->insert(value);
        }
        setStatus("Empleado guardado.");
        refreshStaff();
    } catch (const std::exception& error) {
        setStatus(error.what(), true);
    }
}

void SettingsWorkspaceScreen::saveRole() {
    NexoBarImp app;
    try {
        if (m_roleName->text.empty())
            throw std::runtime_error("El nombre del rol es obligatorio.");
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
        setStatus("Rol guardado.");
        refreshStaff();
    } catch (const std::exception& error) {
        setStatus(error.what(), true);
    }
}

void SettingsWorkspaceScreen::toggleUser() {
    NexoBarImp app;
    try {
        if (!m_userSelect->selected())
            throw std::runtime_error("Selecciona un empleado.");
        auto value = app->database()->get<nexobar::User>(
            selectedId<nexobar::UserId>(m_userSelect));
        value.active = !value.active;
        app->database()->update(value);
        setStatus(value.active ? "Empleado activado." : "Empleado desactivado.");
        refreshStaff();
    } catch (const std::exception& e) {
        setStatus(e.what(), true);
    }
}

void SettingsWorkspaceScreen::toggleRole() {
    NexoBarImp app;
    try {
        if (!m_roleSelect->selected())
            throw std::runtime_error("Selecciona un rol.");
        auto value = app->database()->get<nexobar::Role>(
            selectedId<nexobar::RoleId>(m_roleSelect));
        value.active = !value.active;
        app->database()->update(value);
        setStatus(value.active ? "Rol activado." : "Rol desactivado.");
        refreshStaff();
    } catch (const std::exception& e) {
        setStatus(e.what(), true);
    }
}

void SettingsWorkspaceScreen::removeUser() {
    NexoBarImp app;
    try {
        if (!m_userSelect->selected())
            throw std::runtime_error("Selecciona un empleado.");
        app->database()->remove<nexobar::User>(
            selectedId<nexobar::UserId>(m_userSelect));
        setStatus("Empleado eliminado.");
        refreshStaff();
    } catch (const std::exception& e) {
        setStatus("No se puede eliminar: " + std::string(e.what()), true);
    }
}

void SettingsWorkspaceScreen::removeRole() {
    NexoBarImp app;
    try {
        if (!m_roleSelect->selected())
            throw std::runtime_error("Selecciona un rol.");
        app->database()->remove<nexobar::Role>(
            selectedId<nexobar::RoleId>(m_roleSelect));
        setStatus("Rol eliminado.");
        refreshStaff();
    } catch (const std::exception& e) {
        setStatus("No se puede eliminar: " + std::string(e.what()), true);
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
    setStatus(assigned
                  ? "El permiso está asignado al rol."
                  : "El permiso no está asignado al rol.");
}

void SettingsWorkspaceScreen::updatePermission(const bool assign) {
    NexoBarImp app;
    try {
        if (!m_roleSelect->selected() || !m_permissionSelect->selected())
            throw std::runtime_error("Selecciona un rol y un permiso.");
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
        setStatus(error.what(), true);
    }
}

void SettingsWorkspaceScreen::buildBusiness() {
    NexoBarImp app;
    const auto& business = app->business();
    const std::vector<std::pair<std::string, std::string> > values = {
        {"Razón social", business.legalName},
        {"Nombre comercial", business.tradeName},
        {"NIF", business.nif},
        {"Dirección", business.address},
        {"Código postal", business.postalCode},
        {"Ciudad", business.city},
        {"Provincia", business.province},
        {"País", business.country},
        {"Teléfono", business.phone.value_or("")},
        {"Correo electrónico", business.email.value_or("")},
        {"Licencia", business.license}
    };
    for (const auto& [label, value]: values) {
        addLabel(label);
        addField(label, value);
    }
    addActions({}, {{"Guardar configuración", [this] { saveBusiness(); }}});
}

void SettingsWorkspaceScreen::saveBusiness() {
    NexoBarImp app;
    try {
        if (m_fields.size() != 11)
            throw std::runtime_error("Formulario de negocio incompleto.");
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
        setStatus("Configuración del negocio guardada.");
    } catch (const std::exception& error) {
        setStatus(error.what(), true);
    }
}

void SettingsWorkspaceScreen::buildTemplate() {
    m_templatePath = activeTemplatePath().string();
    addLabel("Contenido de la plantilla");
    auto editor = addField("Plantilla del ticket", {}, true, 330);
    std::ifstream input(m_templatePath, std::ios::binary);
    std::ostringstream data;
    data << input.rdbuf();
    editor->setText(data.str());
    addActions({}, {
                   {"Guardar", [this] { saveTemplate(); }},
                   {"Recargar", [this] { reloadTemplate(); }},
                   {"Imprimir prueba", [this] { printTemplateTest(false); }},
                   {"Prueba + cajón", [this] { printTemplateTest(true); }}
               });
    m_summary->text = "Edita texto y placeholders. La prueba usa los datos "
            "actuales del negocio y el ancho de papel detectado.";
    m_root->content.add(m_summary, LayoutSize::fill(), LayoutSize::px(55));
}

void SettingsWorkspaceScreen::saveTemplate() {
    try {
        std::ofstream output(m_templatePath, std::ios::binary | std::ios::trunc);
        if (!output)
            throw std::runtime_error("No se pudo abrir la plantilla para escribir.");
        output << m_fields.front()->text;
        if (!output)
            throw std::runtime_error("No se pudo guardar la plantilla.");
        setStatus("Plantilla guardada.");
    } catch (const std::exception& error) {
        setStatus(error.what(), true);
    }
}

void SettingsWorkspaceScreen::reloadTemplate() {
    try {
        std::ifstream input(m_templatePath, std::ios::binary);
        if (!input)
            throw std::runtime_error("No se pudo abrir la plantilla.");
        std::ostringstream data;
        data << input.rdbuf();
        m_fields.front()->setText(data.str());
        setStatus("Plantilla recargada.");
    } catch (const std::exception& error) {
        setStatus(error.what(), true);
    }
}

void SettingsWorkspaceScreen::printTemplateTest(const bool drawer) {
    NexoBarImp app;
    try {
        nexobar::Ticket ticket;
        ticket.fullNumber = "PRUEBA-0001";
        ticket.seriesCode = "PRUEBA";
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
        line.description = "Ticket de prueba";
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
        setStatus(ok ? "Ticket de prueba enviado." : error, !ok);
    } catch (const std::exception& error) {
        setStatus(error.what(), true);
    }
}

void SettingsWorkspaceScreen::buildStatus() {
    m_templatePath = activeTemplatePath().string();
    m_root->content.add(m_summary, LayoutSize::fill(), LayoutSize::px(205));
    addActions("Dispositivo",
               {{"Actualizar", [this] { refreshDeviceStatus(); }}});
    addActions("Pruebas",
               {
                   {"Ticket", [this] { printTemplateTest(false); }},
                   {
                       "Cajón",
                       [this] {
                           NexoBarImp app;
                           std::string error;
                           const bool ok =
                                   PrinterDiagnostics::testDrawer(error, *app->i18n());
                           setStatus(ok ? "Cajón activado." : error, !ok);
                       }
                   },
                   {"Ticket + cajón", [this] { printTemplateTest(true); }}
               });
    refreshDeviceStatus();
}

void SettingsWorkspaceScreen::refreshDeviceStatus() {
    NexoBarImp app;
    License license;
    std::ostringstream output;
    output << "Licencia: "
            << license.typeName(license.validate(app->business().license)) << "\n";
    std::string error;
    const auto printers = PrinterDiagnostics::enumerateUsbPrinters(error);
    if (!error.empty())
        output << "Impresora: error de detección (" << error << ")\n";
    else if (printers.empty())
        output << "Impresora: no se detectó ninguna cola USB compatible\n";
    for (const auto& printer: printers) {
        output << "Impresora: " << printer.name
                << (printer.isDefault ? " (predeterminada)" : "")
                << "\nPuerto: " << printer.port << "\nDriver: "
                << (printer.driver.empty() ? "no informado" : printer.driver)
                << "\nPapel: ";
        if (printer.paperWidthMm)
            output << printer.paperWidthMm << " mm";
        else
            output << "no informado por el driver";
        if (printer.paperHeightMm)
            output << " x " << printer.paperHeightMm << " mm";
        output << "\nComandos cargados: " << printer.commandSet;
        if (printer.dpi)
            output << " · " << printer.dpi << " dpi";
        output << "\n";
    }
    m_summary->text = output.str();
    setStatus("Estado actualizado.");
}

void SettingsWorkspaceScreen::buildHistory() {
    m_root->content.add(m_summary, LayoutSize::fill(), LayoutSize::px(235));
    addActions("Periodo", {
                   {
                       "Día",
                       [this] {
                           m_historyPeriod = HistoryPeriod::Day;
                           refreshHistory();
                       }
                   },
                   {
                       "Mes",
                       [this] {
                           m_historyPeriod = HistoryPeriod::Month;
                           refreshHistory();
                       }
                   },
                   {
                       "Todo", [this] {
                           m_historyPeriod = HistoryPeriod::All;
                           refreshHistory();
                       }
                   }
               });
    addActions("Fecha", {
                   {"Anterior", [this] { moveHistory(-1); }},
                   {
                       "Hoy",
                       [this] {
                           m_historyDate =
                                   boost::gregorian::day_clock::local_day();
                           refreshHistory();
                       }
                   },
                   {"Siguiente", [this] { moveHistory(1); }}
               });
    addActions("Informe",
               {
                   {"Imprimir", [this] { printHistory(false); }},
                   {"Cerrar día e imprimir", [this] { printHistory(true); }}
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
    output << "CIERRE / INFORME DE FACTURACIÓN\n";
    if (m_historyPeriod == HistoryPeriod::Day)
        output << "Día: " << boost::gregorian::to_iso_extended_string(m_historyDate)
                << "\n";
    else if (m_historyPeriod == HistoryPeriod::Month)
        output << "Mes: " << m_historyDate.year() << "-" << std::setw(2)
                << std::setfill('0') << static_cast<int>(m_historyDate.month())
                << std::setfill(' ') << "\n";
    else
        output << "Periodo: histórico completo\n";
    output << "Tickets: " << count
            << "\nBase imponible: " << nexobar::formatMoney(base, *app->i18n())
            << "\nImpuestos: " << nexobar::formatMoney(tax, *app->i18n())
            << "\nTOTAL: " << nexobar::formatMoney(total, *app->i18n())
            << "\nEfectivo: " << nexobar::formatMoney(cash, *app->i18n())
            << "\nTarjeta: " << nexobar::formatMoney(card, *app->i18n())
            << "\n--------------------------------\n"
            << detail.str();
    return output.str();
}

void SettingsWorkspaceScreen::refreshHistory() {
    try {
        m_summary->text = historyReport();
        setStatus("Histórico actualizado.");
    } catch (const std::exception& error) {
        setStatus(error.what(), true);
    }
}

void SettingsWorkspaceScreen::printHistory(const bool closeDay) {
    NexoBarImp app;
    try {
        if (closeDay && m_historyPeriod != HistoryPeriod::Day)
            throw std::runtime_error(
                "Selecciona la vista por día para realizar el cierre.");
        TicketPrinter printer;
        std::string error;
        if (!printer.printText(historyReport(), error, *app->i18n()))
            throw std::runtime_error(error);
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
                      ? "Cierre impreso. Sesiones de caja cerradas: " +
                        std::to_string(closed)
                      : "Informe impreso.");
    } catch (const std::exception& error) {
        setStatus(error.what(), true);
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
    m_root->update(deltaSeconds);
}

void SettingsWorkspaceScreen::draw() {
    layout();
    m_root->draw();
    for (const auto& dropdown: m_dropdowns)
        dropdown->drawOverlay();
}
