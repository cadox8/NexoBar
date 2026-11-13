#pragma once

#include "../../components/layout/BlockComponent.h"
#include "../../components/settings/SettingsActionRow.h"
#include "../../components/text/StaticText.h"
#include "../IScreen.h"
#include "../ScreenLayout.h"

#include <functional>
#include <string>

class SettingsHomeScreen final : public IScreen {
public:
    using PermissionCheck = std::function<bool(const std::string &)>;

    explicit SettingsHomeScreen(PermissionCheck hasPermission);

    void update(float deltaSeconds) override;

    void draw() override;

private:
    void layout();

    void open(const IScreenPtr& screen);

    BlockComponentPtr m_root;
    StaticTextPtr m_title, m_subtitle;
    SettingsActionRowPtr m_primaryRow, m_secondaryRow;
    ScreenLayout m_pages;
    FontPtr m_font;
    bool m_pageOpen = false;
    bool m_hasSecondaryRow = false;
};
