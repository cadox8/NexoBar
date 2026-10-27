#pragma once

#include "../core/IComponent.h"

class ProgressBar final : public IComponent {
public:
    float value = 0.f;
    Color track{42, 51, 66, 255};
    Color fill{67, 145, 224, 255};
    float roundness = .25f;
    void update(float) override {}
    void draw() override;
};

using ProgressBarPtr = std::shared_ptr<ProgressBar>;
