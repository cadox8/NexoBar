#pragma once
#include "../core/IComponent.h"
#include "../../../manager/AssetManager.h"
#include <utility>

enum class ImageFit { Contain, Cover, Stretch };

class StaticImage final : public IComponent {
public:
    StaticImage() = default;

    explicit StaticImage(TexturePtr value) : texture(std::move(value)) {}
    TexturePtr texture;
    Color tint = WHITE;
    ImageFit fit = ImageFit::Contain;

    void draw() override;

    void update(float) override {}
};

using StaticImagePtr = std::shared_ptr<StaticImage>;
