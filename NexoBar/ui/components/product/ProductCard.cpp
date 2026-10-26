#include "ProductCard.h"
#include "../core/ScissorMode.h"
#include <algorithm>

void ProductCard::update(float) {
    UIInput::registerKeyboardTarget(this);
    m_hovered = enabled && CheckCollisionPointRec(UIInput::pointerPosition(), m_area);
    if (enabled && !UIInput::clicksSuppressed()
        && ((CheckCollisionPointRec(UIInput::pointerPosition(), m_area) && UIInput::pointerReleased())
            || UIInput::keyboardActivated(this))
        && !outOfStock && onClick)
        onClick();
}

void ProductCard::draw() {
    DrawRectangleRounded(m_area, .12f, 8, m_hovered ? hoverBackground : background);
    DrawRectangleRoundedLinesEx(m_area, .12f, 8, 1.f, Color{74, 87, 105, 255});
    const float textX = m_area.x + 8;
    const float textWidth = std::max(0.f, m_area.width - 16);
    const float footerHeight = fontSize * (price.empty() ? 1.f : 2.f) + 8.f;
    const float imageSide = showImage ? std::max(0.f, std::min(textWidth, m_area.height - footerHeight - 12.f)) : 0.f;
    const Rectangle imageArea{
        m_area.x + (m_area.width - imageSide) * .5f,
        m_area.y + 6,
        imageSide,
        imageSide
    };

    if (showImage&& image && image->id&& imageArea.width > 0) {
        const float scale = std::min(imageArea.width / static_cast<float>(image->width),
                                     imageArea.height / static_cast<float>(image->height));
        const float width = static_cast<float>(image->width) * scale;
        const float height = static_cast<float>(image->height) * scale;
        const Rectangle destination{
            imageArea.x + (imageArea.width - width) * .5f,
            imageArea.y + (imageArea.height - height) * .5f, width, height
        };
        const Rectangle source{0, 0, static_cast<float>(image->width), static_cast<float>(image->height)};
        DrawTexturePro(*image, source, destination, {0, 0}, 0, WHITE);
    }

    const float nameY = m_area.y + m_area.height - footerHeight;
    const auto drawLine = [&](const std::string& value, float y, Color color) {
        if (value.empty())
            return;
        std::string fitted = value;
        const auto measure = [&](const std::string& text) { return font ? MeasureTextEx(*font, text.c_str(), static_cast<float>(fontSize), 1).x : static_cast<float>(MeasureText(text.c_str(), fontSize)); };
        while (!fitted.empty() && measure(fitted) > textWidth - 6.f) {
            std::size_t start = fitted.size() - 1;
            while (start > 0 && (static_cast<unsigned char>(fitted[start]) & 0xC0) == 0x80)--start;
            fitted.resize(start);
        }
        if (fitted != value && textWidth > 20.f)fitted += "...";
        const float measuredWidth = font
                                        ? MeasureTextEx(*font, fitted.c_str(), static_cast<float>(fontSize), 1).x
                                        : static_cast<float>(MeasureText(fitted.c_str(), fontSize));
        const float x = textX + (textWidth - measuredWidth) * .5f;
        if (font)
            DrawTextEx(*font, fitted.c_str(), {x, y}, static_cast<float>(fontSize), 1, color);
        else
            DrawText(fitted.c_str(), static_cast<int>(x), static_cast<int>(y), fontSize, color);
    };
    UI::beginScissor({textX, nameY, textWidth, footerHeight});
    drawLine(name, nameY, textColor);
    drawLine(price, nameY + fontSize + 2.f, priceColor);
    UI::endScissor();
    if (outOfStock) {
        DrawRectangleRec(m_area, Color{20, 24, 31, 150});
        const char* label = "AGOTADO";
        const float width = font
                                ? MeasureTextEx(*font, label, static_cast<float>(fontSize), 1).x
                                : static_cast<float>(MeasureText(label, fontSize));
        const Vector2 position{
            m_area.x + (m_area.width - width) * .5f,
            m_area.y + (m_area.height - static_cast<float>(fontSize)) * .5f
        };
        if (font)
            DrawTextEx(*font, label, position, static_cast<float>(fontSize), 1, RAYWHITE);
        else
            DrawText(label, static_cast<int>(position.x), static_cast<int>(position.y), fontSize, RAYWHITE);
    }
    if (!enabled)DrawRectangleRec(m_area, Color{20, 24, 31, 120});
}
