#pragma once

#include "../../components/box/BoxComponent.h"
#include "../../components/navigation/NavigationButton.h"
#include "../IScreen.h"

#include <functional>
#include <string>
#include <vector>

class SideMenuScreen final : public IScreen {
public:
    struct Item {
        std::string id;
        std::string label;
        std::string icon;
        std::function<void()> action;
        bool bottom = false;
        bool enabled = true;
    };

    explicit SideMenuScreen(std::vector<Item> items);
    void select(const std::string& id);
    void update(float deltaSeconds) override;
    void draw() override;

private:
    void layout();
    BoxComponentPtr m_background;
    std::vector<Item> m_items;
    std::vector<NavigationButtonPtr> m_buttons;
    std::string m_selected;
};
