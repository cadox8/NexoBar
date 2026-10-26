#include "DropdownComponent.h"
#include <algorithm>

void DropdownComponent::setOptions(std::vector<DropdownOption> values, bool preserve) {
    std::string old;
    const bool hadSelection = preserve && selected();
    if (hadSelection)old = selected()->value;
    options = std::move(values);
    selectedIndex = -1;
    if (hadSelection)
        for (std::size_t i = 0; i < options.size(); ++i)
            if (options[i].value == old) {
                selectedIndex = int(i);
                break;
            }
    if (selectedIndex < 0 && !options.empty())selectedIndex = 0;
    m_firstVisible = 0;
}

const DropdownOption* DropdownComponent::selected() const noexcept { return selectedIndex >= 0 && selectedIndex < int(options.size()) ? &options[std::size_t(selectedIndex)] : nullptr; }

void DropdownComponent::select(int index) {
    if (index < 0 || index >= int(options.size()) || index == selectedIndex)return;
    selectedIndex = index;
    const auto selectedOption = options[std::size_t(index)];
    if (index < m_firstVisible)m_firstVisible = index;
    else {
        const int visible = std::max(1, int(popupArea().height / std::max(1.f, optionHeight)));
        if (index >= m_firstVisible + visible)m_firstVisible = index - visible + 1;
    }
    if (onSelectionChanged)onSelectionChanged(index, selectedOption);
}

Rectangle DropdownComponent::popupArea() const {
    const float safeOptionHeight = std::max(1.f, optionHeight);
    const int visibleByScreen = std::max(1, int(GetScreenHeight() / safeOptionHeight));
    const int shown = std::min({std::max(1, maxVisibleOptions), int(options.size()), visibleByScreen});
    const float h = safeOptionHeight * shown;
    const float below = GetScreenHeight() - (m_area.y + m_area.height);
    const float y = std::clamp(below >= h ? m_area.y + m_area.height : m_area.y - h, 0.f, std::max(0.f, float(GetScreenHeight()) - h));
    return {m_area.x, y, m_area.width, h};
}

void DropdownComponent::update(float) {
    if (!enabled)return;
    const Vector2 mouse = UIInput::pointerPosition();
    m_hovered = -1;
    if (open) {
        const Rectangle popup = popupArea();
        if (CheckCollisionPointRec(mouse, popup)) {
            const int row = int((mouse.y - popup.y) / std::max(1.f, optionHeight)) + m_firstVisible;
            if (row >= 0 && row < int(options.size()))m_hovered = row;
            const float wheel = UIInput::consumeMouseWheel();
            const int visible = std::max(1, int(popup.height / std::max(1.f, optionHeight)));
            if (wheel != 0)m_firstVisible = std::clamp(m_firstVisible - int(wheel), 0, std::max(0, int(options.size()) - visible));
        }
    }
    if (UIInput::pointerReleased()) {
        if (CheckCollisionPointRec(mouse, m_area)) {
            open = !open;
            if (open)m_hovered = selectedIndex;
            return;
        }
        if (open&& m_hovered>=0) {
            select(m_hovered);
            open = false;
            return;
        }
        if (open && !CheckCollisionPointRec(mouse, popupArea()))open = false;
    }
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))open = !open;
    if (open && !options.empty() && IsKeyPressed(KEY_DOWN)) { select(std::min(int(options.size()) - 1, selectedIndex + 1)); }
    if (open && !options.empty() && IsKeyPressed(KEY_UP)) { select(std::max(0, selectedIndex - 1)); }
}

void DropdownComponent::draw() {
    DrawRectangleRec(m_area, enabled ? background : Color{54, 61, 72, 255});
    DrawRectangleLinesEx(m_area, 1, enabled ? borderColor : Color{73, 80, 91, 255});
    if (const auto* s = selected()) {
        if (font)DrawTextEx(*font, s->label.c_str(), {m_area.x + 10, m_area.y + (m_area.height - fontSize) * .5f}, float(fontSize), 1, textColor);
        else DrawText(s->label.c_str(), int(m_area.x + 10), int(m_area.y + (m_area.height - fontSize) * .5f), fontSize, textColor);
    }
    if (!enabled) DrawRectangleRec(m_area, Color{0, 0, 0, 65});
    const char* arrow = open ? "^" : "v";
    if (font) DrawTextEx(*font, arrow, {m_area.x + m_area.width - 24, m_area.y + (m_area.height - fontSize) * .5f}, float(fontSize), 1, textColor);
    else DrawText(arrow, int(m_area.x + m_area.width - 24), int(m_area.y + (m_area.height - fontSize) * .5f), fontSize, textColor);
}

void DropdownComponent::drawOverlay() {
    if (!open || !enabled)return;
    const Rectangle popup = popupArea();
    BeginScissorMode(int(popup.x), int(popup.y), int(popup.width), int(popup.height));
    const int visible = std::max(1, int(popup.height / std::max(1.f, optionHeight)));
    for (int i = 0; i < std::min(visible, int(options.size()) - m_firstVisible); ++i) {
        const int index = m_firstVisible + i;
        Rectangle row{popup.x, popup.y + optionHeight * i, popup.width, std::max(1.f, optionHeight)};
        DrawRectangleRec(row, index == m_hovered ? hoverBackground : popupBackground);
        const auto& label = options[std::size_t(index)].label;
        if (font)DrawTextEx(*font, label.c_str(), {row.x + 10, row.y + (row.height - fontSize) * .5f}, float(fontSize), 1, textColor);
        else DrawText(label.c_str(), int(row.x + 10), int(row.y + (row.height - fontSize) * .5f), fontSize, textColor);
    }
    EndScissorMode();
}
