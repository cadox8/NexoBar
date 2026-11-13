#pragma once

#include "../ScreenLayout.h"
#include "../menus/SideMenuScreen.h"
#include "../sales/ProductState.h"

#include <functional>
#include <memory>
#include <string>

class MainScreen final : public ScreenLayout {
public:
    MainScreen(std::shared_ptr<ProductState> state, std::function<void()> logout);

private:
    std::shared_ptr<ScreenLayout> m_content;
    std::shared_ptr<SideMenuScreen> m_menu;
};
