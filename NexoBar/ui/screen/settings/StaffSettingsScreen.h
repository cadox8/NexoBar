#pragma once

#include "../../components/layout/BlockComponent.h"
#include "../../components/settings/SettingsActionRow.h"
#include "../../components/text/StaticText.h"
#include "../IScreen.h"
#include "../ScreenLayout.h"

#include <functional>

class StaffSettingsScreen final : public IScreen {
public:
    explicit StaffSettingsScreen(std::function<void()> back);

    void update(float deltaSeconds) override;

    void draw() override;

private:
    void layout();

    void openUsers();

    void openRoles();

    void openPermissions();

    std::function<void()> m_back;
    BlockComponentPtr m_root;
    StaticTextPtr m_title, m_description;
    SettingsActionRowPtr m_navigation, m_backRow;
    ScreenLayout m_pages;
    FontPtr m_font;
    bool m_detailOpen = false;
};
