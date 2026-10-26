#pragma once
#include "../core/IComponent.h"
#include "../../../manager/AssetManager.h"
#include <functional>
#include <string>

class CheckBox final : public IComponent {
public:
    std::string label;
    FontPtr font;
    int fontSize = 20;
    bool checked = false, focused = false;
    Color background{35, 42, 54, 255}, borderColor{92, 106, 124, 255};
    Color checkColor{80, 190, 150, 255}, textColor = RAYWHITE;
    std::function<void(bool)> onChanged;
    explicit CheckBox(std::string text = {}, bool value = false) : label(std::move(text)), checked(value) {}

    void setChecked(bool value, bool notify = true);

    void update(float deltaSeconds) override;

    void draw() override;
};

using CheckBoxPtr = std::shared_ptr<CheckBox>;
