#include "MessageScreen.h"

#include <raylib.h>

#include <algorithm>
#include <sstream>
#include <utility>
#include <vector>

MessageScreen::MessageScreen(std::string title, std::vector<std::string> errors, std::vector<std::string> warnings, std::string errorPrefix, std::string warningPrefix, FontPtr font)
    : m_title(std::move(title))
      , m_errors(std::move(errors))
      , m_warnings(std::move(warnings))
      , m_errorPrefix(std::move(errorPrefix))
      , m_warningPrefix(std::move(warningPrefix))
      , m_font(std::move(font)) {}

void MessageScreen::update(float) {}

void MessageScreen::draw() {
    const int width = GetScreenWidth();
    const int height = GetScreenHeight();
    const int margin = std::max(24, width / 12);
    const int maxTextWidth = std::max(100, width - margin * 2);
    constexpr int fontSize = 22;
    constexpr int lineHeight = 32;

    std::vector<std::pair<std::string, Color> > lines;
    const auto measure = [this](const std::string& value, const int size) {
        return m_font ? MeasureTextEx(*m_font, value.c_str(), static_cast<float>(size), 1).x : static_cast<float>(MeasureText(value.c_str(), size));
    };
    const auto appendWrapped = [&](const std::string& message, const Color color) {
        std::istringstream words(message);
        std::string word, line;
        while (words >> word) {
            const std::string candidate = line.empty() ? word : line + " " + word;
            if (!line.empty() && measure(candidate, fontSize) > maxTextWidth) {
                lines.emplace_back(std::move(line), color);
                line = std::move(word);
            } else {
                line = candidate;
            }
        }
        if (!line.empty())
            lines.emplace_back(std::move(line), color);
    };
    for (const std::string& error: m_errors)
        appendWrapped(m_errorPrefix + error, Color{255, 170, 160, 255});
    for (const std::string& warning: m_warnings)
        appendWrapped(m_warningPrefix + warning, Color{255, 205, 125, 255});

    const int blockHeight = 58 + static_cast<int>(lines.size()) * lineHeight;
    int y = std::max(24, (height - blockHeight) / 2);
    const float titleWidth = measure(m_title, 34);
    const Color titleColor = m_errors.empty() ? Color{255, 205, 125, 255} : Color{255, 130, 120, 255};
    if (m_font)
        DrawTextEx(*m_font, m_title.c_str(), {(width - titleWidth) * .5f, static_cast<float>(y)}, 34, 1, titleColor);
    else
        DrawText(m_title.c_str(), static_cast<int>((width - titleWidth) * .5f), y, 34, titleColor);
    y += 58;
    for (const auto& [text, color]: lines) {
        const float textWidth = measure(text, fontSize);
        const float x = std::max(static_cast<float>(margin), (width - textWidth) * .5f);
        if (m_font)
            DrawTextEx(*m_font, text.c_str(), {x, static_cast<float>(y)}, static_cast<float>(fontSize), 1, color);
        else
            DrawText(text.c_str(), static_cast<int>(x), y, fontSize, color);
        y += lineHeight;
    }
}
