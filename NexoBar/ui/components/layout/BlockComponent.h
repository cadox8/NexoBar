#pragma once
#include "../box/BoxComponent.h"
#include "Layout.h"
#include <utility>

class BlockComponent : public BoxComponent {
public:
    explicit BlockComponent(LayoutAxis axis = LayoutAxis::Vertical, Color background = BLANK) : BoxComponent(background), content(axis) {}
    Layout content;

    BlockComponent& add(ComponentPtr child, LayoutOptions options = {}) {
        content.add(std::move(child), options);
        return *this;
    }

    BlockComponent& add(ComponentPtr child, LayoutSize width, LayoutSize height, LayoutInsets margin = {}) {
        content.add(std::move(child), width, height, margin);
        return *this;
    }

    BlockComponent& addWeighted(ComponentPtr child, float weight) {
        content.addWeighted(std::move(child), weight);
        return *this;
    }

    BlockComponent& addPixels(ComponentPtr child, float width, float height, LayoutInsets margin = {}) {
        content.addPixels(std::move(child), width, height, margin);
        return *this;
    }

    BlockComponent& addPercent(ComponentPtr child, float width, float height, LayoutInsets margin = {}) {
        content.addPercent(std::move(child), width, height, margin);
        return *this;
    }

    void update(float deltaSeconds) override;

    void draw() override;
};

using BlockComponentPtr = std::shared_ptr<BlockComponent>;
