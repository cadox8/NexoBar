#pragma once
#include "IScreen.h"
#include "../components/layout/BlockComponent.h"

class ScreenBase : public IScreen {
public:
    explicit ScreenBase(LayoutAxis axis = LayoutAxis::Vertical,
                        Color background = Color{26, 33, 44, 255})
        : root(std::make_shared<BlockComponent>(axis, background)) {}

    ScreenBase& add(const ComponentPtr& component, LayoutOptions options = {}) {
        root->content.add(component, options);
        return *this;
    }

    ScreenBase& add(const ComponentPtr& component, LayoutSize width, LayoutSize height,
                    LayoutInsets margin = {}) {
        root->content.add(component, width, height, margin);
        return *this;
    }

    ScreenBase& gap(float value) { root->content.gap(value); return *this; }
    ScreenBase& padding(LayoutInsets value) { root->content.padding(value); return *this; }
    ScreenBase& grid(std::vector<float> rows, std::vector<float> columns) {
        root->content.grid(std::move(rows), std::move(columns));
        return *this;
    }

    void update(const float dt) override {
        if (root) {
            root->area(m_area);
            root->update(dt);
        }
    }

    void draw() override {
        if (root)
            root->draw();
    }

protected:
    BlockComponentPtr root;
};

using ScreenBasePtr = std::shared_ptr<ScreenBase>;
