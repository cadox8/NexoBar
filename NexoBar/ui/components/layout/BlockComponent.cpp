#include "BlockComponent.h"

void BlockComponent::update(float dt) {
    content.setArea(m_area);
    content.update(dt);
}

void BlockComponent::draw() {
    BoxComponent::draw();
    content.setArea(m_area);
    content.draw();
}
