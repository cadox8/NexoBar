#pragma once
#include "IScreen.h"
#include "../components/layout/BlockComponent.h"

class ScreenBase : public IScreen {
public:
    ScreenBase() : root(std::make_shared<BlockComponent>()) {}

    void update(const float dt) override {
        if (root) {
            root->area({.x = 0, .y = 0, .width = static_cast<float>(GetScreenWidth()), .height = static_cast<float>(GetScreenHeight())});
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
