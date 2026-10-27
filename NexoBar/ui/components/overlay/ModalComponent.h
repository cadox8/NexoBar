#pragma once

#include "../core/IComponent.h"

class ModalComponent final : public IComponent {
public:
    explicit ModalComponent(ComponentPtr content = {}) : content(std::move(content)) {}
    ComponentPtr content;
    Color backdrop{0, 0, 0, 175};
    Color panel{27, 34, 45, 255};
    float widthRatio = .8f;
    float heightRatio = .8f;
    void update(float deltaSeconds) override;
    void draw() override;
private:
    Rectangle panelArea() const noexcept;
};

using ModalComponentPtr = std::shared_ptr<ModalComponent>;
