#pragma once
#include "../core/IComponent.h"
#include <functional>

class BoxComponent : public IComponent {
public:
    explicit BoxComponent(Color background = BLANK) : background(background) {}
    Color background, borderColor = BLANK;
    float borderWidth = 0, roundness = 0;
    int segments = 8;
    void update(float) override {}

    void draw() override;
};

using BoxComponentPtr = std::shared_ptr<BoxComponent>;

class RoundedBoxComponent : public BoxComponent {
public:
    RoundedBoxComponent(Color background = BLANK, float roundness = .15f, int segments = 8) : BoxComponent(background) {
        this->roundness = roundness;
        this->segments = segments;
    }
};

using RoundedBoxComponentPtr = std::shared_ptr<RoundedBoxComponent>;

class HoverBoxComponent : public BoxComponent {
public:
    HoverBoxComponent(Color normal, Color hover) : BoxComponent(normal), normalColor(normal), hoverColor(hover) {}
    Color normalColor, hoverColor;
    bool hovered = false;
    std::function<void()> onClick;

    void update(float) override;

    void draw() override;
};

using HoverBoxComponentPtr = std::shared_ptr<HoverBoxComponent>;

class HoverRoundedBoxComponent : public HoverBoxComponent {
public:
    HoverRoundedBoxComponent(Color normal, Color hover, float roundness = .15f, int segments = 8) : HoverBoxComponent(normal, hover) {
        this->roundness = roundness;
        this->segments = segments;
    }
};

using HoverRoundedBoxComponentPtr = std::shared_ptr<HoverRoundedBoxComponent>;
