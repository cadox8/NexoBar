#include "InputText.h"
#include "../core/ScissorMode.h"
#include <algorithm>
#include <cmath>

static std::size_t utf8Count(const std::string& value) {
    std::size_t count = 0;
    for (unsigned char byte: value)if ((byte & 0xC0) != 0x80)++count;
    return count;
}

static std::size_t previous(const std::string& value, std::size_t position) {
    if (position == 0)return 0;
    --position;
    while (position > 0 && (static_cast<unsigned char>(value[position]) & 0xC0) == 0x80)--position;
    return position;
}

static std::size_t next(const std::string& value, std::size_t position) {
    if (position >= value.size())return value.size();
    ++position;
    while (position < value.size() && (static_cast<unsigned char>(value[position]) & 0xC0) == 0x80)++position;
    return position;
}

static void appendCodepoint(std::string& value, int codepoint) {
    if (codepoint <= 0x7f)value.push_back(char(codepoint));
    else if (codepoint <= 0x7ff) {
        value.push_back(char(0xc0 | (codepoint >> 6)));
        value.push_back(char(0x80 | (codepoint & 0x3f)));
    } else if (codepoint <= 0xffff) {
        value.push_back(char(0xe0 | (codepoint >> 12)));
        value.push_back(char(0x80 | ((codepoint >> 6) & 0x3f)));
        value.push_back(char(0x80 | (codepoint & 0x3f)));
    } else if (codepoint <= 0x10ffff) {
        value.push_back(char(0xf0 | (codepoint >> 18)));
        value.push_back(char(0x80 | ((codepoint >> 12) & 0x3f)));
        value.push_back(char(0x80 | ((codepoint >> 6) & 0x3f)));
        value.push_back(char(0x80 | (codepoint & 0x3f)));
    }
}

static float measure(const InputText& component, const std::string& value) {
    if (component.font)return MeasureTextEx(*component.font, value.c_str(), float(component.fontSize), 1).x;
    return float(MeasureText(value.c_str(), component.fontSize));
}

void InputText::setText(std::string value) {
    if (utf8Count(value) > maxLength) {
        std::size_t position = 0, count = 0;
        while (position < value.size() && count < maxLength) {
            position = next(value, position);
            ++count;
        }
        value.resize(position);
    }
    text = std::move(value);
    m_cursor = text.size();
    if (onChange)onChange(text);
}

void InputText::update(float deltaSeconds) {
    m_caretTime += deltaSeconds;
    if (UIInput::pointerPressed()) {
        focused = enabled && CheckCollisionPointRec(UIInput::pointerPosition(), m_area);
        if (focused) {
            const float lineHeight = float(fontSize) * 1.25f;
            const int targetLine = multiline ? std::max(0, int((UIInput::pointerPosition().y - m_area.y - 5) / lineHeight)) : 0;
            std::size_t lineStart = 0;
            for (int line = 0; line < targetLine; ++line) {
                const auto newline = text.find('\n', lineStart);
                if (newline == std::string::npos)break;
                lineStart = newline + 1;
            }
            std::size_t lineEnd = text.find('\n', lineStart);
            if (lineEnd == std::string::npos)lineEnd = text.size();
            const float targetX = UIInput::pointerPosition().x - m_area.x - 8 + (multiline ? 0.f : m_textScroll);
            m_cursor = lineStart;
            float previousWidth = 0;
            for (std::size_t position = lineStart; position < lineEnd;) {
                const std::size_t following = next(text, position);
                const float currentWidth = measure(*this, text.substr(lineStart, following - lineStart));
                if (targetX < (previousWidth + currentWidth) * .5f)break;
                m_cursor = following;
                previousWidth = currentWidth;
                position = following;
            }
            if (targetX >= previousWidth)m_cursor = lineEnd;
        }
    }
    if (!focused || !enabled)return;

    bool changed = false, submitted = false;
    if (IsKeyPressed(KEY_BACKSPACE) && m_cursor > 0) {
        const std::size_t position = previous(text, m_cursor);
        text.erase(position, m_cursor - position);
        m_cursor = position;
        changed = true;
    }
    if (IsKeyPressed(KEY_DELETE) && m_cursor < text.size()) {
        const std::size_t position = next(text, m_cursor);
        text.erase(m_cursor, position - m_cursor);
        changed = true;
    }
    if (IsKeyPressed(KEY_LEFT))m_cursor = previous(text, m_cursor);
    if (IsKeyPressed(KEY_RIGHT))m_cursor = next(text, m_cursor);

    const std::size_t currentLineStart = text.rfind('\n', m_cursor == 0 ? 0 : m_cursor - 1);
    const std::size_t lineStart = currentLineStart == std::string::npos ? 0 : currentLineStart + 1;
    const std::size_t lineEndFound = text.find('\n', m_cursor);
    const std::size_t lineEnd = lineEndFound == std::string::npos ? text.size() : lineEndFound;
    if (IsKeyPressed(KEY_HOME))m_cursor = lineStart;
    if (IsKeyPressed(KEY_END))m_cursor = lineEnd;

    if (multiline && (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_DOWN))) {
        const std::size_t column = utf8Count(text.substr(lineStart, m_cursor - lineStart));
        if (IsKeyPressed(KEY_UP) && lineStart > 0) {
            const std::size_t previousEnd = lineStart - 1;
            const std::size_t previousBreak = previousEnd == 0 ? std::string::npos : text.rfind('\n', previousEnd - 1);
            const std::size_t previousStart = previousBreak == std::string::npos ? 0 : previousBreak + 1;
            m_cursor = previousStart;
            for (std::size_t i = 0; i < column && m_cursor < previousEnd; ++i)m_cursor = next(text, m_cursor);
        } else if (IsKeyPressed(KEY_DOWN) && lineEnd < text.size()) {
            const std::size_t nextStart = lineEnd + 1;
            const std::size_t nextEndFound = text.find('\n', nextStart);
            const std::size_t nextEnd = nextEndFound == std::string::npos ? text.size() : nextEndFound;
            m_cursor = nextStart;
            for (std::size_t i = 0; i < column && m_cursor < nextEnd; ++i)m_cursor = next(text, m_cursor);
        }
    }

    int codepoint = GetCharPressed();
    while (codepoint > 0) {
        if (codepoint == 10 || codepoint == 13) {
            if (multiline&& utf8Count(text) < maxLength) {
                text.insert(m_cursor, 1, '\n');
                ++m_cursor;
                changed = true;
            } else submitted = true;
        } else if (codepoint >= 32 && codepoint <= 0x10ffff && utf8Count(text) < maxLength) {
            std::string encoded;
            appendCodepoint(encoded, codepoint);
            text.insert(m_cursor, encoded);
            m_cursor += encoded.size();
            changed = true;
        }
        codepoint = GetCharPressed();
    }
    if (!multiline && IsKeyPressed(KEY_ENTER))submitted = true;
    if (submitted && onSubmit)onSubmit(text);
    if (changed && onChange)onChange(text);
}

void InputText::draw() {
    DrawRectangleRec(m_area, background);
    DrawRectangleLinesEx(m_area, borderWidth, focused ? focusColor : borderColor);
    UI::beginScissor({
        m_area.x + 5, m_area.y + 2,
        std::max(0.f, m_area.width - 10), std::max(0.f, m_area.height - 4)
    });

    std::string displayed = text;
    if (password) {
        displayed.clear();
        for (unsigned char byte: text) {
            if (byte == '\n')displayed.push_back('\n');
            else if ((byte & 0xC0) != 0x80)displayed.push_back('*');
        }
    }
    const bool showPlaceholder = displayed.empty() && !focused;
    const std::string value = showPlaceholder ? placeholder : displayed;
    const Color color = showPlaceholder ? placeholderColor : textColor;
    const float lineHeight = float(fontSize) * 1.25f;
    std::size_t cursorLineStart = 0;
    for (std::size_t position = 0; position < m_cursor && position < text.size(); ++position)
        if (text[position] == '\n')cursorLineStart = position + 1;
    const std::string cursorPrefix = text.substr(cursorLineStart, m_cursor - cursorLineStart);
    const std::string shownPrefix = password ? std::string(utf8Count(cursorPrefix), '*') : cursorPrefix;
    const float caretOffset = measure(*this, shownPrefix);
    const int caretRow = int(std::count(text.begin(), text.begin() + static_cast<std::ptrdiff_t>(std::min(m_cursor, text.size())), '\n'));

    if (!multiline) {
        if (focused&& caretOffset -m_textScroll > m_area.width - 16)m_textScroll = caretOffset - (m_area.width - 16);
        else if (caretOffset < m_textScroll)m_textScroll = caretOffset;
    } else m_textScroll = 0;

    float y = m_area.y + (multiline ? 5.f : (m_area.height - fontSize) * .5f);
    std::size_t begin = 0;
    do {
        const std::size_t endFound = value.find('\n', begin);
        const std::size_t end = endFound == std::string::npos ? value.size() : endFound;
        const std::string line = value.substr(begin, end - begin);
        const Vector2 position{m_area.x + 8 - m_textScroll, y};
        if (font)DrawTextEx(*font, line.c_str(), position, float(fontSize), 1, color);
        else DrawText(line.c_str(), int(position.x), int(position.y), fontSize, color);
        y += lineHeight;
        begin = end + 1;
        if (endFound == std::string::npos)break;
    } while (begin <= value.size() && multiline);

    if (focused&& std::fmod(m_caretTime, 1.f) < .5f) {
        const float caretX = m_area.x + 8 + caretOffset - m_textScroll;
        const float caretY = m_area.y + (multiline ? 5.f + caretRow * lineHeight : 6.f);
        DrawLine(int(caretX), int(caretY), int(caretX), int(caretY + lineHeight - 2), textColor);
    }
    UI::endScissor();
}
