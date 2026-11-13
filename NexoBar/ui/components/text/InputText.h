#pragma once
#include "../core/IComponent.h"
#include "../../../manager/AssetManager.h"
#include <functional>
#include <string>

class InputText final : public IComponent {
public:
    std::string text, placeholder;
    FontPtr font;
    std::size_t maxLength = 128;
    int fontSize = 22;
    bool password = false, multiline = false, focused = false;
    Color background{35, 42, 54, 255}, borderColor{85, 96, 112, 255}, focusColor{65, 150, 220, 255}, textColor = RAYWHITE, placeholderColor{145, 155, 170, 255};
    float borderWidth = 1;
    std::function<void(const std::string &)> onChange;
    std::function<void(const std::string &)> onSubmit;

    void setText(std::string value);
    void insertVirtual(char value);
    void backspaceVirtual();

    void update(float deltaSeconds) override;

    void draw() override;

private:
    float m_caretTime = 0, m_textScroll = 0;
    std::size_t m_cursor = 0;
};

using InputTextPtr = std::shared_ptr<InputText>;
