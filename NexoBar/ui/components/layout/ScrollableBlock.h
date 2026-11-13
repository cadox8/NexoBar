#pragma once
#include "BlockComponent.h"
#include <algorithm>

class ScrollableBlock : public BlockComponent {
public:
    explicit ScrollableBlock(LayoutAxis axis = LayoutAxis::Vertical, Color background = BLANK) : BlockComponent(axis, background) {}
    float wheelSpeed = 48;
    bool showScrollbar = true, dragScrollbar = true;
    float scrollOffset() const noexcept { return m_scroll; }
    float contentExtent() const noexcept { return m_contentExtent; }
    void scrollTo(float offset) noexcept { m_scroll = std::max(0.f, offset); }

    void update(float deltaSeconds) override;

    void draw() override;

private:
    float m_scroll = 0, m_contentExtent = 0, m_dragOffset = 0;
    float m_lastPointerCoordinate = 0, m_pointerStartCoordinate = 0;
    bool m_draggingScrollbar = false;
    bool m_pointerWasDown = false;
    bool m_draggingContent = false;
    bool m_contentDragMoved = false;
};

using ScrollableBlockPtr = std::shared_ptr<ScrollableBlock>;
