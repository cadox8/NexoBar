#include "ModalComponent.h"

#include <algorithm>

Rectangle ModalComponent::panelArea() const noexcept {
    const float width = m_area.width * std::clamp(widthRatio, .1f, 1.f);
    const float height = m_area.height * std::clamp(heightRatio, .1f, 1.f);
    return {m_area.x + (m_area.width - width) * .5f, m_area.y + (m_area.height - height) * .5f, width, height};
}
void ModalComponent::update(const float deltaSeconds) {
    if (!visible || !content) return;
    content->area(panelArea());
    content->update(deltaSeconds);
}
void ModalComponent::draw() {
    if (!visible) return;
    DrawRectangleRec(m_area, backdrop);
    DrawRectangleRounded(panelArea(), .035f, 8, panel);
    if (content) { content->area(panelArea()); content->draw(); }
}
