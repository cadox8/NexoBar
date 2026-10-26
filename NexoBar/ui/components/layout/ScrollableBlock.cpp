#include "ScrollableBlock.h"
#include "../core/ScissorMode.h"
#include <algorithm>
#include <cmath>

static Rectangle scrollbarThumb(const Rectangle& area, bool horizontal, float extent, float scroll) {
    const float view = horizontal ? area.width : area.height;
    const float thumbLength = std::min(view, std::max(22.f, view * view / std::max(view, extent)));
    const float travel = std::max(0.f, view - thumbLength);
    const float maximumScroll = std::max(0.f, extent - view);
    const float position = maximumScroll > 0 ? travel * std::clamp(scroll / maximumScroll, 0.f, 1.f) : 0.f;
    return horizontal
               ? Rectangle{area.x + position, area.y + area.height - 6, thumbLength, 5}
               : Rectangle{area.x + area.width - 6, area.y + position, 5, thumbLength};
}

void ScrollableBlock::update(float dt) {
    const bool horizontal = content.direction() == LayoutAxis::Horizontal;
    const float viewport = horizontal ? m_area.width : m_area.height;
    m_contentExtent = std::max(viewport, content.requiredExtent(viewport));
    const float maxScroll = std::max(0.f, m_contentExtent - viewport);
    m_scroll = std::clamp(m_scroll, 0.f, maxScroll);

    const int touchCount = GetTouchPointCount();
    const bool touchDown = touchCount > 0;
    const bool pointerDown = touchDown || IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    const bool pointerPressed = (touchDown && !m_pointerWasDown) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    const bool pointerReleased = !pointerDown && m_pointerWasDown;
    const Vector2 pointer = touchDown ? GetTouchPosition(0) : GetMousePosition();
    const float pointerCoordinate = horizontal ? pointer.x : pointer.y;
    const Rectangle thumb = scrollbarThumb(m_area, horizontal, m_contentExtent, m_scroll);
    const Rectangle hitbox = horizontal
                                 ? Rectangle{m_area.x, m_area.y + m_area.height - 12, m_area.width, 12}
                                 : Rectangle{m_area.x + m_area.width - 12, m_area.y, 12, m_area.height};

    if (pointerPressed && maxScroll > 0 && CheckCollisionPointRec(pointer, m_area)
        && !CheckCollisionPointRec(pointer, hitbox)) {
        m_draggingContent = true;
        m_contentDragMoved = false;
        m_lastPointerCoordinate = pointerCoordinate;
        m_pointerStartCoordinate = pointerCoordinate;
    }
    if (m_draggingContent && pointerDown) {
        const float delta = pointerCoordinate - m_lastPointerCoordinate;
        if (std::abs(delta) > 1.f) {
            m_scroll = std::clamp(m_scroll - delta, 0.f, maxScroll);
            m_contentDragMoved = m_contentDragMoved || std::abs(pointerCoordinate - m_pointerStartCoordinate) >= 6.f;
            m_lastPointerCoordinate = pointerCoordinate;
        }
        if (m_contentDragMoved)
            UIInput::suppressClicks();
    }
    if (m_draggingContent && pointerReleased) {
        m_draggingContent = false;
    }
    m_pointerWasDown = pointerDown;

    const Vector2 mouse = pointer;
    if (showScrollbar&& dragScrollbar && m_contentExtent > viewport) {
        if (pointerPressed && CheckCollisionPointRec(mouse, hitbox)) {
            m_draggingScrollbar = true;
            const float position = horizontal ? mouse.x : mouse.y;
            const float origin = horizontal ? thumb.x : thumb.y;
            if (CheckCollisionPointRec(mouse, thumb))m_dragOffset = position - origin;
            else {
                const float view = viewport;
                m_scroll = std::clamp(m_scroll + (position < origin ? -view : view), 0.f, maxScroll);
                const Rectangle updatedThumb = scrollbarThumb(m_area, horizontal, m_contentExtent, m_scroll);
                m_dragOffset = position - (horizontal ? updatedThumb.x : updatedThumb.y);
            }
        }
        if (m_draggingScrollbar && pointerDown) {
            const float thumbLength = horizontal ? thumb.width : thumb.height;
            const float travel = std::max(1.f, viewport - thumbLength);
            const float origin = horizontal ? (mouse.x - m_dragOffset - m_area.x) : (mouse.y - m_dragOffset - m_area.y);
            m_scroll = std::clamp(origin / travel * maxScroll, 0.f, maxScroll);
        }
        if (pointerReleased)m_draggingScrollbar = false;
    }

    Rectangle contentArea = m_area;
    if (horizontal) {
        contentArea.x -= m_scroll;
        contentArea.width = m_contentExtent;
    } else {
        contentArea.y -= m_scroll;
        contentArea.height = m_contentExtent;
    }
    content.setArea(contentArea);
    content.update(dt);
    if (!m_draggingScrollbar && !m_draggingContent && CheckCollisionPointRec(mouse, m_area))
        m_scroll = std::clamp(m_scroll - UIInput::consumeMouseWheel() * wheelSpeed, 0.f, maxScroll);
}

void ScrollableBlock::draw() {
    BoxComponent::draw();
    UI::beginScissor({m_area.x, m_area.y, std::max(0.f, m_area.width), std::max(0.f, m_area.height)});
    content.draw();
    UI::endScissor();

    const bool horizontal = content.direction() == LayoutAxis::Horizontal;
    const float viewport = horizontal ? m_area.width : m_area.height;
    if (showScrollbar&& m_contentExtent > viewport)DrawRectangleRec(scrollbarThumb(m_area, horizontal, m_contentExtent, m_scroll), Color{130, 145, 165, 210});
}
