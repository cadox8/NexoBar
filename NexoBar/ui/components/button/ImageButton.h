#pragma once
#include "Button.h"

class ImageButton final : public Button {
public:
    ImageButton(TexturePtr normal, TexturePtr hover) : m_normal(std::move(normal)), m_hover(std::move(hover)) {}

    void draw() override;

private:
    TexturePtr m_normal, m_hover;
};

using ImageButtonPtr = std::shared_ptr<ImageButton>;
