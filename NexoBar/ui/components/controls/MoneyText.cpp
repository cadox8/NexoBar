#include "MoneyText.h"
#include <algorithm>

std::string MoneyText::formatted() const {
    const unsigned precision = std::min(decimals, 18u);
    const bool negative = amountMinor < 0;
    const std::uint64_t magnitude = negative ? std::uint64_t(-(amountMinor + 1)) + 1 : std::uint64_t(amountMinor);
    std::uint64_t divisor = 1;
    for (unsigned i = 0; i < precision; ++i)divisor *= 10;
    std::string whole = std::to_string(magnitude / divisor);
    for (std::ptrdiff_t position = std::ptrdiff_t(whole.size()) - 3; position > 0; position -= 3)
        whole.insert(std::size_t(position), thousandsSeparator);
    std::string result = (negative ? "-" : "") + whole;
    if (precision > 0) {
        std::string fraction = std::to_string(magnitude % divisor);
        if (fraction.size() < precision)fraction.insert(0, precision - fraction.size(), '0');
        result += decimalSeparator + fraction;
    }
    if (!currency.empty())result += " " + currency;
    return result;
}

void MoneyText::draw() {
    const std::string value = formatted();
    const Vector2 size = font
                             ? MeasureTextEx(*font, value.c_str(), fontSize, 1)
                             : Vector2{float(MeasureText(value.c_str(), int(fontSize))), fontSize};
    const Vector2 position{
        rightAligned ? m_area.x + m_area.width - size.x : m_area.x,
        m_area.y + (m_area.height - size.y) * .5f
    };
    if (font)DrawTextEx(*font, value.c_str(), position, fontSize, 1, color);
    else DrawText(value.c_str(), int(position.x), int(position.y), int(fontSize), color);
}
