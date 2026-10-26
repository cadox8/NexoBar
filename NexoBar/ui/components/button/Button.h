#pragma once
#include "../core/IComponent.h"
#include "../../../manager/AssetManager.h"
#include "../layout/Layout.h"
#include <functional>
#include <string>
#include <utility>

class Button : public IComponent {
public:
    Button() = default;

    explicit Button(std::string label) : text(std::move(label)) {}
    Button(std::string label, std::function<void()> action) : text(std::move(label)), m_onClick(std::move(action)) {}
    std::string text;
    TexturePtr image;
    FontPtr font;
    SoundPtr clickSound;
    int fontSize = 20;
    float roundness = .12f;
    LayoutInsets padding = LayoutInsets::symmetric(12, 8);
    Color background{.r = 52, .g = 105, .b = 170, .a = 255}, hoverBackground{70, 132, 205, 255}, pressedBackground{40, 86, 143, 255}, disabledBackground{80, 85, 94, 255};
    Color textColor = RAYWHITE, borderColor = BLANK;
    float borderWidth = 0;
    [[nodiscard]] bool hovered() const noexcept { return m_hovered; }
    void onClick(std::function<void()> callback) { m_onClick = std::move(callback); }
    void onHover(std::function<void(bool)> callback) { m_onHover = std::move(callback); }

    void update(float deltaSeconds) override;

    void draw() override;

protected:
    bool m_hovered = false, m_pressed = false;
    std::function<void()> m_onClick;
    std::function<void(bool)> m_onHover;
};

using ButtonPtr = std::shared_ptr<Button>;
