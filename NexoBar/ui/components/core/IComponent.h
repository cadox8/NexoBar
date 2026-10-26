#pragma once
#include <raylib.h>
#include <memory>
#include <algorithm>
#include <vector>

class UIInput {
public:
    static void beginFrame() noexcept {
        s_mouseWheelConsumed = false;
        s_clicksSuppressed = s_clickSuppressionLatched;
        s_clickSuppressionLatched = false;
        s_keyboardTargets.clear();
        const int touches = GetTouchPointCount();
        s_pointerPosition = touches > 0 ? GetTouchPosition(0) : GetMousePosition();
        s_pointerPressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || (touches > 0 && !s_touchWasDown);
        s_pointerReleased = IsMouseButtonReleased(MOUSE_BUTTON_LEFT) || (touches == 0 && s_touchWasDown);
        s_touchWasDown = touches > 0;
        if (IsKeyPressed(KEY_TAB) && !s_keyboardTargetsPrevious.empty()) {
            const int direction = IsKeyDown(KEY_LEFT_SHIFT) ? -1 : 1;
            const int count = static_cast<int>(s_keyboardTargetsPrevious.size());
            s_focusIndex = ((s_focusIndex % count) + direction + count) % count;
        }
    }

    static void suppressClicks() noexcept {
        s_clicksSuppressed = true;
        s_clickSuppressionLatched = true;
    }

    static bool clicksSuppressed() noexcept { return s_clicksSuppressed; }

    static float consumeMouseWheel() noexcept {
        if (s_mouseWheelConsumed)return 0;
        const float value = GetMouseWheelMove();
        if (value != 0)s_mouseWheelConsumed = true;
        return value;
    }

    static Vector2 pointerPosition() noexcept { return s_pointerPosition; }
    static bool pointerPressed() noexcept { return s_pointerPressed; }
    static bool pointerReleased() noexcept { return s_pointerReleased; }

    static void registerKeyboardTarget(const void* target) {
        s_keyboardTargets.push_back(target);
        if (s_keyboardTargetsPrevious.empty() && s_keyboardTargets.size() == 1) s_focusIndex = 0;
    }

    static bool keyboardActivated(const void* target) {
        const auto it = std::ranges::find(s_keyboardTargets, target);
        if (it == s_keyboardTargets.end()) return false;
        const auto index = static_cast<int>(std::distance(s_keyboardTargets.begin(), it));
        return index == s_focusIndex && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE));
    }

    static bool keyboardFocused(const void* target) {
        const auto it = std::ranges::find(s_keyboardTargets, target);
        return it != s_keyboardTargets.end() && static_cast<int>(std::distance(s_keyboardTargets.begin(), it)) == s_focusIndex;
    }

    static void endFrame() {
        s_keyboardTargetsPrevious = s_keyboardTargets;
        if (s_keyboardTargetsPrevious.empty()) s_focusIndex = 0;
        else s_focusIndex %= static_cast<int>(s_keyboardTargetsPrevious.size());
    }

private:
    static inline bool s_mouseWheelConsumed = false;
    static inline bool s_clicksSuppressed = false;
    static inline bool s_clickSuppressionLatched = false;
    static inline Vector2 s_pointerPosition{};
    static inline bool s_pointerPressed = false, s_pointerReleased = false, s_touchWasDown = false;
    static inline int s_focusIndex = 0;
    static inline std::vector<const void*> s_keyboardTargets, s_keyboardTargetsPrevious;
};

class IComponent {
public:
    virtual ~IComponent() = default;

    virtual void update(float deltaSeconds) = 0;

    virtual void draw() = 0;

    virtual void area(const Rectangle& a) { m_area = a; }
    [[nodiscard]] const Rectangle& area() const noexcept { return m_area; }
    Rectangle& area() noexcept { return m_area; }

    bool visible = true;
    bool enabled = true;

protected:
    Rectangle m_area{};
};

using ComponentPtr = std::shared_ptr<IComponent>;
