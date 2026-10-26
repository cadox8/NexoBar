#pragma once

#include "../../components/layout/BlockComponent.h"
#include "../IScreen.h"

#include <functional>

class SideMenuScreen final : public IScreen {
public:
    SideMenuScreen(std::function<void()> onSales,
                   std::function<void()> onSettings, bool showSettings);

    void update(float deltaSeconds) override;

    void draw() override;

private:
    void layout();

    BlockComponentPtr m_root;
    ComponentPtr m_iconSales, m_iconSettings;
    bool m_showSettings = false;
};
