#include "SettingsActionRow.h"

#include <algorithm>

SettingsActionRow::SettingsActionRow(std::string label, FontPtr font,
                                     std::vector<Action> actions)
    : m_label(std::move(label)), m_font(std::move(font)) {
    m_buttons.reserve(actions.size());
    for (auto& action: actions) {
        auto button = std::make_shared<Button>(std::move(action.text),
                                               std::move(action.callback));
        button->font = m_font;
        button->fontSize = m_fontSize;
        button->roundness = .18f;
        button->padding = LayoutInsets::symmetric(10.f, 6.f);
        button->background = Color{38, 52, 70, 255};
        button->hoverBackground = Color{49, 72, 96, 255};
        button->pressedBackground = Color{31, 44, 60, 255};
        button->borderColor = Color{70, 91, 118, 255};
        button->borderWidth = 1.f;
        if (action.destructive) {
            button->background = Color{151, 55, 61, 255};
            button->hoverBackground = Color{183, 68, 75, 255};
            button->pressedBackground = Color{124, 43, 49, 255};
            button->borderColor = Color{205, 92, 98, 255};
        }
        m_buttons.push_back(std::move(button));
    }
}

void SettingsActionRow::setFontSize(const int value) {
    m_fontSize = value;
    for (const auto& button: m_buttons)
        button->fontSize = value;
}

ButtonPtr SettingsActionRow::button(const std::size_t index) const {
    return index < m_buttons.size() ? m_buttons[index] : nullptr;
}

void SettingsActionRow::layout() {
    const float gap = std::clamp(m_area.height * .12f, 5.f, 10.f);
    const float labelWidth =
            m_label.empty() ? 0.f : std::clamp(m_area.width * .24f, 110.f, 230.f);
    const float buttonsX = m_area.x + labelWidth + (labelWidth > 0 ? gap : 0.f);
    const float available =
            std::max(0.f, m_area.width - labelWidth - (labelWidth > 0 ? gap : 0.f));
    const float width =
            m_buttons.empty()
                ? 0.f
                : std::max(0.f, (available -
                                 gap * static_cast<float>(m_buttons.size() - 1)) /
                                static_cast<float>(m_buttons.size()));
    for (std::size_t i = 0; i < m_buttons.size(); ++i)
        m_buttons[i]->area({
            buttonsX + static_cast<float>(i) * (width + gap),
            m_area.y, width, m_area.height
        });
}

void SettingsActionRow::update(const float deltaSeconds) {
    layout();
    for (const auto& button: m_buttons)
        if (button->visible)
            button->update(deltaSeconds);
}

void SettingsActionRow::draw() {
    layout();
    if (!m_label.empty() && m_font) {
        const Vector2 size = MeasureTextEx(*m_font, m_label.c_str(),
                                           static_cast<float>(m_fontSize), 1.f);
        DrawTextEx(*m_font, m_label.c_str(),
                   {m_area.x, m_area.y + (m_area.height - size.y) * .5f},
                   static_cast<float>(m_fontSize), 1.f, Color{213, 220, 231, 255});
    }
    for (const auto& button: m_buttons)
        if (button->visible)
            button->draw();
}
