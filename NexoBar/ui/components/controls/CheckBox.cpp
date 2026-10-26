#include "CheckBox.h"
#include <algorithm>

void CheckBox::setChecked(bool value, bool notify) {
    if (checked == value)return;
    checked = value;
    if (notify && onChanged)onChanged(checked);
}

void CheckBox::update(float) {
    if (!enabled)return;
    if (UIInput::pointerPressed()) {
        focused = CheckCollisionPointRec(UIInput::pointerPosition(), m_area);
        if (focused)setChecked(!checked);
    }
    if (focused && (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)))setChecked(!checked);
}

void CheckBox::draw() {
    const float side = std::min(m_area.height, 26.f);
    Rectangle box{m_area.x, m_area.y + (m_area.height - side) * .5f, side, side};
    DrawRectangleRec(box, background);
    DrawRectangleLinesEx(box, 1, focused ? checkColor : borderColor);
    if (checked) {
        DrawLineEx({box.x + side * .2f, box.y + side * .53f}, {box.x + side * .43f, box.y + side * .76f}, 3, checkColor);
        DrawLineEx({box.x + side * .43f, box.y + side * .76f}, {box.x + side * .82f, box.y + side * .24f}, 3, checkColor);
    }
    const Vector2 pos{box.x + side + 9, m_area.y + (m_area.height - fontSize) * .5f};
    if (font)DrawTextEx(*font, label.c_str(), pos, float(fontSize), 1, textColor);
    else DrawText(label.c_str(), int(pos.x), int(pos.y), fontSize, textColor);
}
