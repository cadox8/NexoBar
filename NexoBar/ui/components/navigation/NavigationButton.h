#pragma once

#include "../button/Button.h"

class NavigationButton final : public Button {
public:
    using Button::Button;
    bool selected = false;
    Color selectedBackground{67, 105, 160, 255};
    void draw() override;
};

using NavigationButtonPtr = std::shared_ptr<NavigationButton>;
