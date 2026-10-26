#include "SideMenuScreen.h"
#include "../../../NexoBar.h"

#include <algorithm>
#include <cmath>

namespace {
    class IconAction final : public IComponent {
    public:
        explicit IconAction(bool sales, std::function<void()> action)
            : m_sales(sales), m_action(std::move(action)) {
            if (m_sales)
                m_icon = NexoBarImp::get()->assets().getTexture("icon-cart");
        }

        void update(float) override {
            const auto p = UIInput::pointerPosition();
            m_hover = enabled && CheckCollisionPointRec(p, m_area);
            if (enabled && !UIInput::clicksSuppressed() && m_hover &&
                UIInput::pointerReleased() && m_action)
                m_action();
        }

        void draw() override {
            DrawRectangleRounded(m_area, .24f, 6, m_hover ? Color{.r = 57, .g = 73, .b = 94, .a = 255} : Color{.r = 39, .g = 50, .b = 66, .a = 255});
            const float cx = m_area.x + m_area.width * .5f, cy = m_area.y + m_area.height * .5f, s = std::min(m_area.width, m_area.height);
            const auto ink = RAYWHITE;
            if (m_sales) {
                if (m_icon && m_icon->id) {
                    const float side = s * .72f;
                    const Rectangle src{
                        0, 0, static_cast<float>(m_icon->width),
                        static_cast<float>(m_icon->height)
                    };
                    const Rectangle dst{cx - side * .5f, cy - side * .5f, side, side};
                    DrawTexturePro(*m_icon, src, dst, {0, 0}, 0, WHITE);
                }
            } else {
                DrawCircleLines(int(cx), int(cy), s * .18f, ink);
                DrawCircleLines(int(cx), int(cy), s * .065f, ink);
                for (int i = 0; i < 8; ++i) {
                    const float a = i * 3.14159265f / 4;
                    DrawLineEx({cx + std::cos(a) * s * .2f, cy + std::sin(a) * s * .2f},
                               {cx + std::cos(a) * s * .27f, cy + std::sin(a) * s * .27f},
                               2, ink);
                }
            }
        }

    private:
        bool m_sales, m_hover = false;
        std::function<void()> m_action;
        TexturePtr m_icon;
    };
} // namespace

SideMenuScreen::SideMenuScreen(std::function<void()> onSales, std::function<void()> onSettings, const bool showSettings) : m_showSettings(showSettings) {
    m_root = std::make_shared<BlockComponent>(LayoutAxis::Vertical,
                                              Color{22, 28, 38, 255});
    // Icon actions are mounted by the screen directly; the 44 px rail remains
    // independent of content width.
    m_iconSales = std::make_shared<IconAction>(true, std::move(onSales));
    m_iconSettings = std::make_shared<IconAction>(false, std::move(onSettings));
}

void SideMenuScreen::layout() {
    const float side = std::max(
        0.f, std::min({44.f, m_area.width - 8.f, (m_area.height - 12.f) * .5f}));
    m_iconSales->area(
        {m_area.x + (m_area.width - side) * .5f, m_area.y + 4.f, side, side});
    m_iconSettings->area({
        m_area.x + (m_area.width - side) * .5f,
        m_area.y + m_area.height - side - 4.f, side, side
    });
    m_root->area(m_area);
}

void SideMenuScreen::update(float dt) {
    layout();
    m_iconSales->update(dt);
    if (m_showSettings)
        m_iconSettings->update(dt);
}

void SideMenuScreen::draw() {
    layout();
    m_root->draw();
    m_iconSales->draw();
    if (m_showSettings)
        m_iconSettings->draw();
}
