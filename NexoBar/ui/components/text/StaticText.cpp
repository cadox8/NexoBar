#include "StaticText.h"
#include "../core/ScissorMode.h"
#include <algorithm>
#include <sstream>
#include <vector>

static Vector2 measure(const StaticText& component, const std::string& value) {
    if (component.font)return MeasureTextEx(*component.font, value.c_str(), component.fontSize, component.lineSpacing);
    return {float(MeasureText(value.c_str(), int(component.fontSize))), component.fontSize};
}

static void drawTextLine(const StaticText& component, const std::string& value, Vector2 position) {
    if (component.font)DrawTextEx(*component.font, value.c_str(), position, component.fontSize, component.lineSpacing, component.color);
    else DrawText(value.c_str(), int(position.x), int(position.y), int(component.fontSize), component.color);
}

static std::size_t nextUtf8Boundary(const std::string& value, std::size_t position) {
    ++position;
    while (position < value.size() && (static_cast<unsigned char>(value[position]) & 0xC0) == 0x80)++position;
    return position;
}

static std::string fitWithEllipsis(const StaticText& component, std::string value, float maxWidth) {
    if (measure(component, value).x <= maxWidth)return value;
    constexpr const char* ellipsis = "...";
    while (!value.empty() && measure(component, value + ellipsis).x > maxWidth) {
        std::size_t start = value.size() - 1;
        while (start > 0 && (static_cast<unsigned char>(value[start]) & 0xC0) == 0x80)--start;
        value.resize(start);
    }
    return value + ellipsis;
}

static void wrapParagraph(const StaticText& component, const std::string& paragraph, std::vector<std::string>& lines) {
    const float availableWidth = component.area().width;
    if (!component.wrap || availableWidth <= 0) {
        lines.push_back(paragraph);
        return;
    }
    std::istringstream words(paragraph);
    std::string word, line;
    while (words >> word) {
        const std::string candidate = line.empty() ? word : line + " " + word;
        if (measure(component, candidate).x <= availableWidth) {
            line = candidate;
            continue;
        }
        if (!line.empty()) {
            lines.push_back(std::move(line));
            line.clear();
        }
        if (measure(component, word).x <= availableWidth) {
            line = word;
            continue;
        }
        std::size_t begin = 0;
        while (begin < word.size()) {
            std::size_t end = begin;
            std::string chunk;
            while (end < word.size()) {
                const std::size_t next = nextUtf8Boundary(word, end);
                if (measure(component, word.substr(begin, next - begin)).x > availableWidth && end > begin)break;
                end = next;
                chunk = word.substr(begin, end - begin);
                if (measure(component, chunk).x > availableWidth)break;
            }
            if (end == begin)end = nextUtf8Boundary(word, begin);
            chunk = word.substr(begin, end - begin);
            if (end < word.size())lines.push_back(std::move(chunk));
            else line = std::move(chunk);
            begin = end;
        }
    }
    if (!line.empty() || paragraph.empty())lines.push_back(std::move(line));
}

void StaticText::draw() {
    std::vector<std::string> lines;
    std::istringstream paragraphs(text);
    std::string paragraph;
    while (std::getline(paragraphs, paragraph))wrapParagraph(*this, paragraph, lines);
    if (lines.empty())lines.emplace_back();

    const float lineHeight = std::max(1.f, fontSize * 1.2f);
    const std::size_t maxLines = std::max<std::size_t>(1, static_cast<std::size_t>(std::max(0.f, m_area.height) / lineHeight));
    const bool clippedLines = lines.size() > maxLines;
    if (clippedLines)lines.resize(maxLines);
    if (ellipsis&& clippedLines &&!lines.empty())lines.back() = fitWithEllipsis(*this, std::move(lines.back()), m_area.width);

    const float totalHeight = lineHeight * float(lines.size());
    float y = m_area.y;
    if (verticalAlignment == TextVerticalAlignment::Center)y += (m_area.height - totalHeight) * .5f;
    else if (verticalAlignment == TextVerticalAlignment::Bottom)y += m_area.height - totalHeight;

    UI::beginScissor({m_area.x, m_area.y, std::max(0.f, m_area.width), std::max(0.f, m_area.height)});
    for (std::size_t index = 0; index < lines.size(); ++index) {
        std::string value = std::move(lines[index]);
        if (ellipsis && !wrap)value = fitWithEllipsis(*this, std::move(value), m_area.width);
        const float textWidth = measure(*this, value).x;
        float x = m_area.x;
        if (horizontalAlignment == TextHorizontalAlignment::Center)x += (m_area.width - textWidth) * .5f;
        else if (horizontalAlignment == TextHorizontalAlignment::Right)x += m_area.width - textWidth;
        drawTextLine(*this, value, Vector2{x, y});
        y += lineHeight;
    }
    UI::endScissor();
}
