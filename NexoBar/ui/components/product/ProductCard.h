#pragma once
#include "../core/IComponent.h"
#include "../../../manager/AssetManager.h"
#include <functional>
#include <string>

// Product tile designed for the catalog area of a POS screen.
class ProductCard final : public IComponent {
public:
    std::string name, price, detail, outOfStockLabel;
    TexturePtr image;
    FontPtr font;
    int fontSize = 22;
    bool outOfStock = false;
    bool showImage = true;
    Color background{42, 50, 64, 255}, hoverBackground{56, 69, 88, 255};
    Color textColor = RAYWHITE, priceColor{126, 225, 185, 255};
    std::function<void()> onClick;

    ProductCard() = default;

    ProductCard(std::string label, std::string priceLabel, std::function<void()> action = {})
        : name(std::move(label)), price(std::move(priceLabel)), onClick(std::move(action)) {}

    void update(float deltaSeconds) override;

    void draw() override;

private:
    bool m_hovered = false;
};

using ProductCardPtr = std::shared_ptr<ProductCard>;
