#include "Button.h"
#include "../core/IComponent.h"
#include <algorithm>

void Button::update(float) {
    const bool previous = m_hovered;
    UIInput::registerKeyboardTarget(this);
    const bool focused = UIInput::keyboardFocused(this);
    m_hovered = enabled && CheckCollisionPointRec(UIInput::pointerPosition(), m_area);
    m_pressed = enabled && ((m_hovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) || (focused && IsKeyDown(KEY_ENTER)));
    if (previous != m_hovered && m_onHover)m_onHover(m_hovered);
    if (enabled && !UIInput::clicksSuppressed()
        && ((CheckCollisionPointRec(UIInput::pointerPosition(), m_area) && UIInput::pointerReleased())
            || UIInput::keyboardActivated(this))) {
        if (clickSound)PlaySound(*clickSound);
        if (m_onClick)m_onClick();
    }
}

void Button::draw() {
    Color fill = !enabled ? disabledBackground : (m_pressed ? pressedBackground : (m_hovered ? hoverBackground : background));
    if (roundness > 0)DrawRectangleRounded(m_area, roundness, 8, fill);
    else DrawRectangleRec(m_area, fill);
    if (borderWidth > 0)DrawRectangleLinesEx(m_area, borderWidth, borderColor);
    Rectangle inner{.x = m_area.x + padding.left, .y = m_area.y + padding.top, .width = std::max(0.f, m_area.width - padding.left - padding.right), .height = std::max(0.f, m_area.height - padding.top - padding.bottom)};
    const bool hasVectorIcon = icon != ButtonIcon::None;
    const float iconSize = text.empty() ? std::min(inner.width, inner.height) : std::min(inner.height, float(fontSize) * 1.35f);
    float textX = inner.x;
    if (image&& image->id) {
        Rectangle src{0, 0, float(image->width), float(image->height)}, dst{inner.x, inner.y + (inner.height - iconSize) * .5f, iconSize, iconSize};
        DrawTexturePro(*image, src, dst, {0, 0}, 0, WHITE);
        textX += iconSize + (text.empty() ? 0 : 8);
    } else if (hasVectorIcon) {
        const float size = iconSize * .58f;
        const float x = inner.x + (inner.width - size) * .5f;
        const float y = inner.y + (inner.height - size) * .5f;
        if (icon == ButtonIcon::Plus) {
            const float thickness = std::max(2.f, size * .12f);
            DrawRectangleRounded({x + (size - thickness) * .5f, y, thickness, size},
                                 .45f, 5, textColor);
            DrawRectangleRounded({x, y + (size - thickness) * .5f, size, thickness},
                                 .45f, 5, textColor);
        }
    }
    if (!text.empty()) {
        if (font) {
            const auto measure = MeasureTextEx(*font, text.c_str(), float(fontSize), 1);
            DrawTextEx(*font, text.c_str(), {textX + (inner.width - (textX - inner.x) - measure.x) * .5f, inner.y + (inner.height - measure.y) * .5f}, float(fontSize), 1, textColor);
        } else {
            const int w = MeasureText(text.c_str(), fontSize);
            DrawText(text.c_str(), int(textX + (inner.width - (textX - inner.x) - w) * .5f), int(inner.y + (inner.height - fontSize) * .5f), fontSize, textColor);
        }
    }
}
