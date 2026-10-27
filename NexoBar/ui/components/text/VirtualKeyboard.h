#pragma once
#include "InputText.h"
#include <functional>

class VirtualKeyboard final : public IComponent {
public:
    explicit VirtualKeyboard(std::function<InputTextPtr()> target) : m_target(std::move(target)) {}
    FontPtr font;
    void update(float) override;
    void draw() override;
private:
    Rectangle keyArea(std::size_t index) const;
    std::function<InputTextPtr()> m_target;
    std::string m_keys{"1234567890QWERTYUIOPASDFGHJKLZXCVBNM"};
};
using VirtualKeyboardPtr = std::shared_ptr<VirtualKeyboard>;
