#include "SideMenuScreen.h"

#include "../../../NexoBar.h"

#include <algorithm>

SideMenuScreen::SideMenuScreen(std::vector<Item> items)
    : m_background(std::make_shared<BoxComponent>(Color{20, 37, 58, 255})),
      m_items(std::move(items)) {
    NexoBarImp nexo;
    for (const auto& item : m_items) {
        auto button = std::make_shared<NavigationButton>(item.label, item.action);
        if (!item.icon.empty()) button->image = nexo->assets().getTexture(item.icon);
        button->font = nexo->defaultFont();
        button->fontSize = 0;
        button->text.clear();
        button->enabled = item.enabled;
        button->roundness = .18f;
        button->padding = LayoutInsets::all(13.f);
        button->background = Color{32, 54, 82, 255};
        button->hoverBackground = Color{47, 72, 105, 255};
        button->pressedBackground = Color{26, 45, 70, 255};
        if (item.id == "logout") {
            button->background = Color{151, 55, 61, 255};
            button->hoverBackground = Color{183, 68, 75, 255};
            button->pressedBackground = Color{124, 43, 49, 255};
            button->selectedBackground = button->background;
        }
        m_buttons.push_back(std::move(button));
    }
    if (!m_items.empty()) select(m_items.front().id);
}

void SideMenuScreen::select(const std::string& id) {
    m_selected = id;
    for (std::size_t i = 0; i < m_items.size(); ++i)
        m_buttons[i]->selected = m_items[i].id == id;
}

void SideMenuScreen::layout() {
    m_background->area(m_area);
    const float inset = 6.f;
    const float gap = 8.f;
    const float height = std::max(42.f, m_area.width - inset * 2.f);
    float top = m_area.y + inset;
    const auto bottomCount = static_cast<float>(std::ranges::count(m_items, true, &Item::bottom));
    float bottom = m_area.y + m_area.height - inset - bottomCount * height - std::max(0.f, bottomCount - 1.f) * gap;
    for (std::size_t i = 0; i < m_items.size(); ++i) {
        const float y = m_items[i].bottom ? bottom : top;
        m_buttons[i]->area({m_area.x + inset, y, std::max(0.f, m_area.width - inset * 2.f), height});
        if (m_items[i].bottom) bottom += height + gap;
        else top += height + gap;
    }
}

void SideMenuScreen::update(const float deltaSeconds) {
    layout();
    for (const auto& button : m_buttons) button->update(deltaSeconds);
}

void SideMenuScreen::draw() {
    layout();
    m_background->draw();
    for (const auto& button : m_buttons) button->draw();
}
