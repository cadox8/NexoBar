#pragma once
#include "../core/IComponent.h"
#include "../../../manager/AssetManager.h"
#include <cstdint>
#include <string>

// Draws currency using integer minor units to avoid floating point rounding.
class MoneyText final : public IComponent {
public:
    std::int64_t amountMinor = 0;
    std::string currency = "€", decimalSeparator = ",", thousandsSeparator = ".";
    unsigned decimals = 2;
    FontPtr font;
    float fontSize = 22;
    Color color = RAYWHITE;
    bool rightAligned = true;

    std::string formatted() const;

    void update(float) override {}

    void draw() override;
};

using MoneyTextPtr = std::shared_ptr<MoneyText>;
