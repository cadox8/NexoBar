#pragma once
#include "../core/IComponent.h"
#include "../../../manager/AssetManager.h"
#include <string>
#include <utility>

enum class TextHorizontalAlignment { Left, Center, Right };

enum class TextVerticalAlignment { Top, Center, Bottom };

class StaticText final : public IComponent {
public:
    StaticText() = default;

    explicit StaticText(std::string value) : text(std::move(value)) {}
    std::string text;
    FontPtr font;
    float fontSize = 22, lineSpacing = 1;
    Color color = RAYWHITE;
    TextHorizontalAlignment horizontalAlignment = TextHorizontalAlignment::Left;
    TextVerticalAlignment verticalAlignment = TextVerticalAlignment::Center;
    bool wrap = true, ellipsis = false;

    void draw() override;

    void update(float) override {}
};

using StaticTextPtr = std::shared_ptr<StaticText>;
