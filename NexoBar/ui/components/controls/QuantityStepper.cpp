#include "QuantityStepper.h"
#include <algorithm>
#include <string>

void QuantityStepper::setValue(int next, bool notify) {
    next = std::clamp(next, std::min(minimum, maximum), std::max(minimum, maximum));
    if (next == value)return;
    value = next;
    if (notify && onChanged)onChanged(value);
}

void QuantityStepper::update(float) {
    if (!enabled)return;
    const float cell = m_area.width / 3.f;
    const Vector2 mouse = GetMousePosition();
    m_hovered = CheckCollisionPointRec(mouse, {m_area.x, m_area.y, cell, m_area.height})
                    ? -1
                    : CheckCollisionPointRec(mouse, {m_area.x + cell * 2, m_area.y, cell, m_area.height})
                          ? 1
                          : 0;
    if (m_hovered != 0 && IsMouseButtonReleased(MOUSE_BUTTON_LEFT))setValue(value + m_hovered);
}

void QuantityStepper::draw() {
    const float cell = m_area.width / 3.f;
    const auto drawCell = [&](float x, const char* text, int action) {
        Rectangle area{x, m_area.y, cell, m_area.height};
        DrawRectangleRec(area, m_hovered == action ? hoverColor : buttonColor);
        DrawRectangleLinesEx(area, 1, Color{91, 105, 123, 255});
        if (font) {
            const Vector2 size = MeasureTextEx(*font, text, float(fontSize), 1);
            DrawTextEx(*font, text, {area.x + (area.width - size.x) * .5f, area.y + (area.height - size.y) * .5f}, float(fontSize), 1, textColor);
        } else {
            const int width = MeasureText(text, fontSize);
            DrawText(text, int(area.x + (area.width - width) * .5f), int(area.y + (area.height - fontSize) * .5f), fontSize, textColor);
        }
    };
    drawCell(m_area.x, "-", -1);
    Rectangle center{m_area.x + cell, m_area.y, cell, m_area.height};
    DrawRectangleRec(center, Color{34, 41, 52, 255});
    const std::string label = std::to_string(value);
    if (font) {
        const Vector2 size = MeasureTextEx(*font, label.c_str(), float(fontSize), 1);
        DrawTextEx(*font, label.c_str(), {center.x + (cell - size.x) * .5f, center.y + (center.height - size.y) * .5f}, float(fontSize), 1, textColor);
    } else {
        const int width = MeasureText(label.c_str(), fontSize);
        DrawText(label.c_str(), int(center.x + (cell - width) * .5f), int(center.y + (center.height - fontSize) * .5f), fontSize, textColor);
    }
    drawCell(m_area.x + cell * 2, "+", 1);
}
