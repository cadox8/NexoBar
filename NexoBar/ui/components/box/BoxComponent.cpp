#include "BoxComponent.h"

void BoxComponent::draw() {
    if (roundness > 0)DrawRectangleRounded(m_area, roundness, segments > 0 ? segments : 8, background);
    else DrawRectangleRec(m_area, background);
    if (borderWidth > 0)DrawRectangleLinesEx(m_area, borderWidth, borderColor);
}

void HoverBoxComponent::update(float) {
    hovered = enabled && CheckCollisionPointRec(GetMousePosition(), m_area);
    background = hovered ? hoverColor : normalColor;
    if (hovered &&IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&!UIInput::clicksSuppressed() && onClick)
    onClick();
}

void HoverBoxComponent::draw() { BoxComponent::draw(); }
