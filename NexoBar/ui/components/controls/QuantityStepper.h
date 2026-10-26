#pragma once
#include "../core/IComponent.h"
#include "../../../manager/AssetManager.h"
#include <functional>

// Compact quantity control for product rows and ticket lines.
class QuantityStepper final : public IComponent {
public:
    int value = 1, minimum = 0, maximum = 999;
    int fontSize = 20;
    FontPtr font;
    Color buttonColor{53, 66, 83, 255}, hoverColor{72, 91, 113, 255}, textColor = RAYWHITE;
    std::function<void(int)> onChanged;

    void setValue(int next, bool notify = true);

    void update(float deltaSeconds) override;

    void draw() override;

private:
    int m_hovered = 0;
};

using QuantityStepperPtr = std::shared_ptr<QuantityStepper>;
