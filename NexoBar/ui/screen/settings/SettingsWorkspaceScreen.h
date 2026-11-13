#pragma once

#include "../../components/controls/DropdownComponent.h"
#include "../../components/layout/ScrollableBlock.h"
#include "../../components/settings/SettingsActionRow.h"
#include "../../components/text/InputText.h"
#include "../../components/text/VirtualKeyboard.h"
#include "../../components/text/StaticText.h"
#include "../../components/ticket/TicketTemplateBlockEditor.h"
#include "../IScreen.h"
#include "database/models/Permission.h"
#include "database/models/Role.h"
#include "database/models/User.h"

#include <boost/date_time/gregorian/gregorian.hpp>
#include <functional>
#include <string>
#include <vector>

class SettingsWorkspaceScreen : public IScreen {
public:
    enum class Kind {
        Users,
        Roles,
        Permissions,
        Business,
        Template,
        Status,
        History
    };

    SettingsWorkspaceScreen(Kind kind, std::function<void()> back);

    void update(float deltaSeconds) override;

    void draw() override;

private:
    enum class HistoryPeriod { Day, Month, All };

    StaticTextPtr addLabel(std::string text, float height = 27.f);

    InputTextPtr addField(std::string placeholder, std::string value = {},
                          bool multiline = false, float height = 44.f);

    DropdownComponentPtr addDropdown(float height = 44.f);

    SettingsActionRowPtr
    addActions(std::string label, std::vector<SettingsActionRow::Action> actions,
               float height = 45.f);

    void setStatus(std::string text, bool error = false);

    void layout();

    void buildStaff();

    void refreshStaff();

    void loadUser();

    void loadRole();

    void saveUser();

    void saveRole();

    void toggleUser();

    void toggleRole();

    void removeUser();

    void removeRole();

    void updatePermission(bool assign);

    void showPermissionState();

    void buildBusiness();

    void saveBusiness();

    void buildTemplate();

    void selectTemplate(bool dailyClose);

    void saveTemplate();

    void reloadTemplate();

    void printTemplateTest(bool drawer);

    void buildStatus();

    void refreshDeviceStatus();

    void buildHistory();

    void moveHistory(int amount);

    void refreshHistory();

    std::string historyReport() const;

    void printHistory(bool closeDay);

    Kind m_kind;
    std::function<void()> m_back;
    ScrollableBlockPtr m_root;
    StaticTextPtr m_title, m_summary, m_status;
    FontPtr m_font;
    std::vector<StaticTextPtr> m_labels;
    std::vector<InputTextPtr> m_fields;
    std::vector<DropdownComponentPtr> m_dropdowns;
    std::vector<SettingsActionRowPtr> m_actionRows;

    DropdownComponentPtr m_userSelect, m_userRoleSelect, m_roleSelect,
            m_permissionSelect, m_businessQrSelect;
    InputTextPtr m_userName, m_userPin, m_roleName, m_roleDescription;
    std::vector<nexobar::User> m_users;
    std::vector<nexobar::Role> m_roles;
    std::vector<nexobar::Permission> m_permissions;
    std::string m_templatePath;
    DropdownComponentPtr m_templateSelect;
    TicketTemplateBlockEditorPtr m_templateBlocks, m_ticketTemplateBlocks, m_closeTemplateBlocks;
    std::size_t m_templateEditorIndex = 0;
    bool m_editingCloseTemplate = false;
    VirtualKeyboardPtr m_virtualKeyboard;
    HistoryPeriod m_historyPeriod = HistoryPeriod::Day;
    boost::gregorian::date m_historyDate;
};
