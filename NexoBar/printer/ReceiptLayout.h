#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

/// Character-column layout for fixed-width receipt printers. Margins are always preserved.
class ReceiptLayout {
public:
    enum class Alignment { Left, Center, Right };

    struct Column {
        std::string text;
        std::size_t width;
        Alignment alignment = Alignment::Left;
    };

    explicit ReceiptLayout(const std::size_t paperColumns = 42, const std::size_t leftMargin = 2, const std::size_t rightMargin = 2) : paperColumns_(paperColumns), leftMargin_(leftMargin), rightMargin_(rightMargin) {}

    [[nodiscard]] std::size_t contentColumns() const noexcept {
        return paperColumns_ > leftMargin_ + rightMargin_ ? paperColumns_ - leftMargin_ - rightMargin_ : 0;
    }

    [[nodiscard]] std::string row(const std::vector<Column>& columns) const {
        const std::size_t available = contentColumns();
        std::string result(leftMargin_, ' ');
        std::size_t remaining = available;
        for (std::size_t i = 0; i < columns.size() && remaining; ++i) {
            // The final column absorbs remaining width so right-aligned content reaches the
            // right margin even when callers only specify widths for earlier columns.
            const std::size_t width = i + 1 == columns.size() ? remaining : std::min(columns[i].width, remaining);
            result += aligned(columns[i].text, width, columns[i].alignment);
            remaining -= width;
        }
        result.append(remaining, ' ');
        result.append(rightMargin_, ' ');
        return result;
    }

    [[nodiscard]] std::string leftRight(std::string_view left, std::string_view right,
                                        std::size_t rightColumns = 12) const {
        const std::size_t available = contentColumns();
        if (!available) return {};
        const std::size_t tail = std::min(rightColumns, available);
        return row({
            {std::string(left), available - tail, Alignment::Left},
            {std::string(right), tail, Alignment::Right}
        });
    }

    [[nodiscard]] std::string left(std::string_view value) const {
        return row({{std::string(value), contentColumns(), Alignment::Left}});
    }

    [[nodiscard]] std::string center(std::string_view value) const {
        return row({{std::string(value), contentColumns(), Alignment::Center}});
    }

    [[nodiscard]] std::string right(std::string_view value) const {
        return row({{std::string(value), contentColumns(), Alignment::Right}});
    }

    [[nodiscard]] std::string separator(const char symbol = '-') const {
        return std::string(leftMargin_, ' ') + std::string(contentColumns(), symbol) + std::string(rightMargin_, ' ');
    }

private:
    static std::size_t displayWidth(std::string_view text) {
        std::size_t count = 0;
        for (const unsigned char ch: text) if ((ch & 0xC0) != 0x80) ++count;
        return count;
    }

    static std::string aligned(std::string_view value, std::size_t width, Alignment alignment) {
        const std::size_t used = displayWidth(value);
        if (used >= width) {
            std::string clipped;
            std::size_t count = 0;
            for (const unsigned char ch: value) {
                if ((ch & 0xC0) != 0x80 && count >= width) break;
                clipped.push_back(static_cast<char>(ch));
                if ((ch & 0xC0) != 0x80) ++count;
            }
            return clipped;
        }
        const std::size_t padding = width - used;
        const std::size_t before = alignment == Alignment::Right ? padding : alignment == Alignment::Center ? padding / 2 : 0;
        return std::string(before, ' ') + std::string(value) + std::string(padding - before, ' ');
    }

    std::size_t paperColumns_;
    std::size_t leftMargin_;
    std::size_t rightMargin_;
};
