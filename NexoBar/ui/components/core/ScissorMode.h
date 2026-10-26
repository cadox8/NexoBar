#pragma once

#include <raylib.h>

#include <algorithm>
#include <vector>

namespace UI {
    inline std::vector<Rectangle>& scissorStack() {
        static thread_local std::vector<Rectangle> stack;
        return stack;
    }

    inline void beginScissor(const Rectangle area) {
        auto& stack = scissorStack();
        Rectangle clipped = area;
        if (!stack.empty()) {
            const Rectangle& parent = stack.back();
            const float left = std::max(area.x, parent.x);
            const float top = std::max(area.y, parent.y);
            const float right = std::min(area.x + area.width, parent.x + parent.width);
            const float bottom = std::min(area.y + area.height, parent.y + parent.height);
            clipped = {left, top, std::max(0.f, right - left), std::max(0.f, bottom - top)};
        }
        stack.push_back(clipped);
        BeginScissorMode(static_cast<int>(clipped.x), static_cast<int>(clipped.y),
                         static_cast<int>(clipped.width), static_cast<int>(clipped.height));
    }

    inline void endScissor() {
        auto& stack = scissorStack();
        if (stack.empty())
            return;
        stack.pop_back();
        if (stack.empty()) {
            EndScissorMode();
            return;
        }
        const Rectangle& parent = stack.back();
        BeginScissorMode(static_cast<int>(parent.x), static_cast<int>(parent.y),
                         static_cast<int>(parent.width), static_cast<int>(parent.height));
    }
}
