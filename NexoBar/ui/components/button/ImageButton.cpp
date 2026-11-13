#include "ImageButton.h"

void ImageButton::draw() {
    image = (m_hovered && m_hover) ? m_hover : m_normal;
    Button::draw();
}
