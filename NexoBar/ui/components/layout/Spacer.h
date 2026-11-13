#pragma once
#include "../core/IComponent.h"

// Invisible flexible/fixed item for keeping deliberate gaps in a Layout.
class Spacer final : public IComponent {
public:
    void update(float) override {}
    void draw() override {}
};

using SpacerPtr = std::shared_ptr<Spacer>;
