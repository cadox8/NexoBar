#pragma once
#include "../../manager/AssetManager.h"

#include <memory>
#include <raylib.h>
#include <string>
#include <raymath.h>


class IScreen {
public:
    virtual ~IScreen() = default;

    virtual void update(float deltaSeconds) = 0;

    virtual void draw() = 0;

    // Screens are independently mounted regions. The default keeps legacy full-window screens working.
    virtual void area(const Rectangle& value) { m_area = value; }
    [[nodiscard]] virtual Rectangle area() const noexcept { return m_area; }

protected:
    void replaceToken(std::string& value, const std::string& token, const std::string& replacement) {
        std::size_t position = 0;

        while ((position = value.find(token, position)) != std::string::npos) {
            value.replace(position, token.size(), replacement);
            position += replacement.size();
        }
    }

protected:
    Color Panel{.r = 26, .g = 33, .b = 44, .a = 255};
    float Padding = 16;

    FontPtr m_font;
    Rectangle m_area{};
};

using IScreenPtr = std::shared_ptr<IScreen>;
