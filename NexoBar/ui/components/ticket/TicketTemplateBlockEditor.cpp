#include "TicketTemplateBlockEditor.h"

#include "../../../NexoBar.h"
#include "database/models/TicketPlaceholder.h"
#include "../core/ScissorMode.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr float HeaderHeight = 36.f;
constexpr float RowHeight = 44.f;
constexpr float RowGap = 7.f;

void drawTriangleUp(const Rectangle area, const Color color) {
    DrawTriangle({area.x + area.width * .5f, area.y + 6.f},
                 {area.x + 6.f, area.y + area.height - 5.f},
                 {area.x + area.width - 6.f, area.y + area.height - 5.f}, color);
}

void drawTriangleDown(const Rectangle area, const Color color) {
    // raylib culls triangles with the opposite winding. Keep the same winding
    // as drawTriangleUp so the downward icon is rendered on every backend.
    DrawTriangle({area.x + area.width * .5f, area.y + area.height - 6.f},
                 {area.x + area.width - 6.f, area.y + 5.f},
                 {area.x + 6.f, area.y + 5.f}, color);
}

std::string localizedLabel(const TicketTemplateBlockEditor::Block& block) {
    NexoBarImp app;
    if (!app || block.label.empty())
        return block.label;
    // label contains either a JSON key (seeded data) or a literal description
    // supplied by a custom database row. I18n returns the input unchanged when
    // it is not a known key, which makes both cases work without exposing the
    // placeholder token.
    return app->text(block.label);
}
}

TicketTemplateBlockEditor::TicketTemplateBlockEditor() {
    NexoBarImp app;
    m_blocks = {
        {"business", "template_blocks.business", "{{business.*}}",
         "{{BUSINESS_NAME}}\n{{business.legal_name}}\n{{business.tax_id}}\n{{business.address}}\n"
         "{{business.postal_code}} {{business.city}}\n{{business.phone}}\n{{business.email}}\n", true},
        {"separator_top", "template_blocks.separator", "{{separator}}", "{{separator}}\n", true},
        {"ticket", "template_blocks.ticket", "{{ticket.number}} · {{ticket.date}}",
         "{{receipt.ticket_label}}: {{ticket.number}}\n{{receipt.date_label}}: {{ticket.date}}\n", true},
        {"separator_lines", "template_blocks.separator", "{{separator}}", "{{separator}}\n", true},
        {"lines", "template_blocks.lines", "{{line.row}}",
         "{{#each lines}}{{line.row}}\n{{/each}}", true},
        {"separator_total", "template_blocks.separator", "{{separator}}", "{{separator}}\n", true},
        {"total", "template_blocks.total", "{{ticket.total}}",
         "{{receipt.total_label}}: {{ticket.total}}\n", true},
        {"footer", "template_blocks.footer", "{{ticket.footer}}", "\n{{ticket.footer}}\n", true}
    };

    if (!app || !app->database())
        return;
    const auto placeholders = app->database()->query<nexobar::TicketPlaceholder>()
        .where(Nearon::col<&nexobar::TicketPlaceholder::active> == true)
        .orderBy(Nearon::col<&nexobar::TicketPlaceholder::sortOrder>.asc())
        .all();
    m_placeholders.reserve(placeholders.size());
    for (const auto& placeholder: placeholders) {
        m_placeholders.push_back({
            "placeholder-" + std::to_string(placeholder.id.get()),
            placeholder.descriptionKey, placeholder.placeholder,
            placeholder.templateSource, true
        });
    }
}

void TicketTemplateBlockEditor::load(const std::string& source) {
    constexpr std::string_view marker = "{{!block:";
    if (source.find(marker) == std::string::npos)
        return; // Legacy template: the constructor already represents its sections.

    const auto knownBlocks = m_blocks;
    std::vector<Block> loaded;
    std::size_t position = 0;
    while ((position = source.find(marker, position)) != std::string::npos) {
        const std::size_t idEnd = source.find("}}", position + marker.size());
        if (idEnd == std::string::npos)
            break;
        std::string id = source.substr(position + marker.size(),
                                       idEnd - position - marker.size());
        constexpr std::string_view sameRowSuffix = "|same-row";
        const bool sameRow = id.ends_with(sameRowSuffix);
        if (sameRow)
            id.resize(id.size() - sameRowSuffix.size());
        std::size_t contentStart = idEnd + 2;
        if (contentStart < source.size() && source[contentStart] == '\n')
            ++contentStart;
        const std::size_t next = source.find(marker, contentStart);
        const std::size_t contentEnd = next == std::string::npos ? source.size() : next;
        std::string content = source.substr(contentStart, contentEnd - contentStart);

        auto found = std::ranges::find(knownBlocks, id, &Block::id);
        if (found != knownBlocks.end()) {
            auto block = *found;
            block.source = std::move(content);
            block.sameRowWithPrevious = sameRow && !loaded.empty();
            loaded.push_back(std::move(block));
        } else {
            const auto placeholder = std::ranges::find_if(m_placeholders, [&id](const Block& item) {
                return id.starts_with(item.id + "-");
            });
            if (placeholder != m_placeholders.end()) {
                auto block = *placeholder;
                block.id = id;
                block.source = std::move(content);
                block.sameRowWithPrevious = sameRow && !loaded.empty();
                loaded.push_back(std::move(block));
            }
        }
        if (next == std::string::npos)
            break;
        position = next;
    }
    if (!loaded.empty())
        m_blocks = std::move(loaded);
    clampScroll();
}

std::string TicketTemplateBlockEditor::templateText() const {
    std::string value;
    for (std::size_t index = 0; index < m_blocks.size(); ++index) {
        const auto& block = m_blocks[index];
        const bool sameRow = index > 0 && block.sameRowWithPrevious;
        if (sameRow) {
            while (!value.empty() && (value.back() == '\n' || value.back() == '\r'))
                value.pop_back();
        } else if (!value.empty() && value.back() != '\n') {
            value += '\n';
        }
        value += "{{!block:" + block.id + (sameRow ? "|same-row" : "") + "}}\n";
        std::string blockSource = block.source;
        if (sameRow) {
            while (!blockSource.empty() && (blockSource.front() == '\n' || blockSource.front() == '\r'))
                blockSource.erase(blockSource.begin());
        }
        value += blockSource;
    }
    return value;
}

Rectangle TicketTemplateBlockEditor::editorArea() const noexcept {
    const float height = std::clamp(m_area.height * .58f, 245.f, 345.f);
    return {m_area.x, m_area.y, m_area.width, std::min(height, m_area.height)};
}

Rectangle TicketTemplateBlockEditor::previewArea() const noexcept {
    const auto editor = editorArea();
    return {m_area.x, editor.y + editor.height + 10.f, m_area.width,
            std::max(0.f, m_area.y + m_area.height - editor.y - editor.height - 10.f)};
}

Rectangle TicketTemplateBlockEditor::columnHeader(const bool available) const noexcept {
    const auto editor = editorArea();
    const float gap = 12.f;
    const float leftWidth = editor.width * .57f - gap * .5f;
    return available
        ? Rectangle{editor.x + leftWidth + gap, editor.y, editor.width - leftWidth - gap, HeaderHeight}
        : Rectangle{editor.x, editor.y, leftWidth, HeaderHeight};
}

Rectangle TicketTemplateBlockEditor::columnViewport(const bool available) const noexcept {
    const auto header = columnHeader(available);
    const auto editor = editorArea();
    return {header.x, header.y + HeaderHeight, header.width,
            std::max(0.f, editor.y + editor.height - header.y - HeaderHeight)};
}

Rectangle TicketTemplateBlockEditor::rowArea(const bool available,
                                              const std::size_t index) const noexcept {
    const auto viewport = columnViewport(available);
    const float scroll = available ? m_placeholderScroll : m_blockScroll;
    return {viewport.x, viewport.y + static_cast<float>(index) * (RowHeight + RowGap) - scroll,
            viewport.width - 9.f, RowHeight};
}

float TicketTemplateBlockEditor::maxScroll(const bool available) const noexcept {
    const std::size_t count = available ? m_placeholders.size() : m_blocks.size();
    const float content = count == 0 ? 0.f : static_cast<float>(count) * (RowHeight + RowGap) - RowGap;
    return std::max(0.f, content - columnViewport(available).height);
}

void TicketTemplateBlockEditor::clampScroll() {
    m_blockScroll = std::clamp(m_blockScroll, 0.f, maxScroll(false));
    m_placeholderScroll = std::clamp(m_placeholderScroll, 0.f, maxScroll(true));
}

void TicketTemplateBlockEditor::update(float) {
    if (!enabled)
        return;
    clampScroll();
    const Vector2 pointer = UIInput::pointerPosition();

    int hoveredColumn = -1;
    if (CheckCollisionPointRec(pointer, columnViewport(false))) hoveredColumn = 0;
    else if (CheckCollisionPointRec(pointer, columnViewport(true))) hoveredColumn = 1;
    if (hoveredColumn >= 0) {
        const float wheel = UIInput::consumeMouseWheel();
        float& scroll = hoveredColumn == 1 ? m_placeholderScroll : m_blockScroll;
        scroll -= wheel * (RowHeight + RowGap);
        clampScroll();
    }

    if (UIInput::pointerPressed() && hoveredColumn >= 0) {
        m_dragColumn = hoveredColumn;
        m_dragLastY = pointer.y;
        m_dragDistance = 0.f;
    }
    const bool pointerDown = GetTouchPointCount() > 0 || IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    if (m_dragColumn >= 0 && pointerDown) {
        const float delta = pointer.y - m_dragLastY;
        float& scroll = m_dragColumn == 1 ? m_placeholderScroll : m_blockScroll;
        scroll -= delta;
        m_dragDistance += std::abs(delta);
        m_dragLastY = pointer.y;
        if (m_dragDistance >= 6.f) UIInput::suppressClicks();
        clampScroll();
    }
    if (!UIInput::pointerReleased())
        return;

    const bool wasDrag = m_dragDistance >= 6.f;
    m_dragColumn = -1;
    m_dragDistance = 0.f;
    if (wasDrag || UIInput::clicksSuppressed() || hoveredColumn < 0)
        return;

    const bool available = hoveredColumn == 1;
    const Rectangle viewport = columnViewport(available);
    const float scroll = available ? m_placeholderScroll : m_blockScroll;
    const float contentY = pointer.y - viewport.y + scroll;
    if (contentY < 0.f)
        return;
    const std::size_t index = static_cast<std::size_t>(contentY / (RowHeight + RowGap));
    const float withinRow = std::fmod(contentY, RowHeight + RowGap);
    if (withinRow > RowHeight)
        return;

    if (!available) {
        if (index >= m_blocks.size()) return;
        const Rectangle area = rowArea(false, index);
        if (pointer.x < area.x + 38.f) {
            if (pointer.y < area.y + area.height * .5f && index > 0)
                std::swap(m_blocks[index], m_blocks[index - 1]);
            else if (pointer.y >= area.y + area.height * .5f && index + 1 < m_blocks.size())
                std::swap(m_blocks[index], m_blocks[index + 1]);
            if (!m_blocks.empty()) m_blocks.front().sameRowWithPrevious = false;
        } else if (pointer.x > area.x + area.width - 44.f && m_blocks[index].removable) {
            m_blocks.erase(m_blocks.begin() + static_cast<std::ptrdiff_t>(index));
            if (!m_blocks.empty()) m_blocks.front().sameRowWithPrevious = false;
        } else if (pointer.x > area.x + area.width - 80.f && index > 0) {
            m_blocks[index].sameRowWithPrevious = !m_blocks[index].sameRowWithPrevious;
        }
        clampScroll();
        return;
    }

    if (index >= m_placeholders.size()) return;
    auto block = m_placeholders[index];
    const std::string baseId = block.id;
    do {
        block.id = baseId + "-" + std::to_string(++m_serial);
    } while (std::ranges::find(m_blocks, block.id, &Block::id) != m_blocks.end());
    m_blocks.push_back(std::move(block));
    m_blockScroll = maxScroll(false);
}

void TicketTemplateBlockEditor::drawHeader(const bool available) const {
    NexoBarImp app;
    const Rectangle header = columnHeader(available);
    const std::string title = app->text(available ? "ticket_editor.available" : "ticket_editor.selected");
    DrawTextEx(*font, title.c_str(), {header.x + 5.f, header.y + 7.f},
               static_cast<float>(fontSize), 1.f, Color{213, 220, 231, 255});
}

void TicketTemplateBlockEditor::drawScrollbar(const bool available) const {
    const Rectangle viewport = columnViewport(available);
    const float maximum = maxScroll(available);
    if (maximum <= 0.f || viewport.height <= 0.f) return;
    const std::size_t count = available ? m_placeholders.size() : m_blocks.size();
    const float content = static_cast<float>(count) * (RowHeight + RowGap) - RowGap;
    const float thumbHeight = std::max(24.f, viewport.height * viewport.height / content);
    const float scroll = available ? m_placeholderScroll : m_blockScroll;
    const float y = viewport.y + (viewport.height - thumbHeight) * (scroll / maximum);
    DrawRectangleRounded({viewport.x + viewport.width - 5.f, y, 4.f, thumbHeight},
                         .8f, 4, Color{132, 153, 181, 220});
}

void TicketTemplateBlockEditor::drawPreview() const {
    const Rectangle preview = previewArea();
    if (preview.height < 35.f)
        return;
    NexoBarImp app;
    DrawRectangleRounded(preview, .025f, 7, Color{248, 245, 235, 255});
    DrawRectangleRoundedLinesEx(preview, .025f, 7, 1.f, Color{190, 184, 170, 255});
    DrawTextEx(*font, app->text("ticket_editor.preview").c_str(),
               {preview.x + 12.f, preview.y + 8.f}, 15.f, 1.f, Color{95, 89, 78, 255});
    float y = preview.y + 32.f;
    const float bottom = preview.y + preview.height - 8.f;
    float previousEndX = preview.x + 18.f;
    float previousY = y;
    bool hasPreviousLine = false;
    for (std::size_t index = 0; index < m_blocks.size(); ++index) {
        const auto& block = m_blocks[index];
        if (y >= bottom)
            break;
        std::string line = localizedLabel(block);
        float size = 15.f;
        bool centered = false;
        if (block.id.starts_with("business")) {
            line = app->text("template_preview.business"); size = 23.f; centered = true;
        } else if (block.id.starts_with("separator")) {
            DrawLineEx({preview.x + 18.f, y + 5.f}, {preview.x + preview.width - 18.f, y + 5.f},
                       1.f, Color{70, 70, 70, 255});
            y += 13.f;
            hasPreviousLine = false;
            continue;
        } else if (block.id.starts_with("ticket")) {
            line = app->text("template_preview.ticket"); size = 15.f;
        } else if (block.id.starts_with("lines")) {
            line = app->text("template_preview.lines"); size = 16.f;
        } else if (block.id.starts_with("total")) {
            line = app->text("template_preview.total"); size = 22.f;
        } else if (block.id.starts_with("footer")) {
            line = app->text("template_preview.footer"); size = 14.f; centered = true;
        } else if (block.token.find("total") != std::string::npos) {
            size = 20.f;
        } else if (block.token.find("business.name") != std::string::npos) {
            size = 21.f; centered = true;
        }
        const bool sameRow = index > 0 && block.sameRowWithPrevious && hasPreviousLine;
        if ((sameRow ? previousY : y) + size > bottom)
            break;
        if (sameRow)
            line = " - " + line;
        float x = sameRow ? previousEndX : preview.x + 18.f;
        if (centered && !sameRow) {
            const float width = MeasureTextEx(*font, line.c_str(), size, 1.f).x;
            x = preview.x + (preview.width - width) * .5f;
        }
        const float drawY = sameRow ? previousY : y;
        DrawTextEx(*font, line.c_str(), {x, drawY}, size, 1.f, Color{28, 28, 28, 255});
        previousEndX = x + MeasureTextEx(*font, line.c_str(), size, 1.f).x;
        if (!sameRow) previousY = drawY;
        y = std::max(y, drawY + size + (size >= 20.f ? 9.f : 5.f));
        hasPreviousLine = true;
    }
}

void TicketTemplateBlockEditor::draw() {
    if (!visible || !font)
        return;
    drawHeader(false);
    drawHeader(true);

    const Rectangle selectedViewport = columnViewport(false);
    UI::beginScissor(selectedViewport);
    for (std::size_t index = 0; index < m_blocks.size(); ++index) {
        const auto& block = m_blocks[index];
        const Rectangle area = rowArea(false, index);
        if (area.y + area.height < selectedViewport.y || area.y > selectedViewport.y + selectedViewport.height)
            continue;
        DrawRectangleRounded(area, .10f, 6, Color{38, 49, 65, 255});
        DrawRectangleRoundedLinesEx(area, .10f, 6, 1.f, Color{92, 110, 136, 255});
        drawTriangleUp({area.x + 8.f, area.y + 3.f, 20.f, 17.f},
                       index > 0 ? RAYWHITE : Color{85, 92, 104, 255});
        drawTriangleDown({area.x + 8.f, area.y + 23.f, 20.f, 17.f},
                         index + 1 < m_blocks.size() ? RAYWHITE : Color{85, 92, 104, 255});
        UI::beginScissor({area.x + 38.f, area.y, std::max(0.f, area.width - 122.f), area.height});
        const float labelSize = static_cast<float>(fontSize - 1);
        const std::string label = localizedLabel(block);
        DrawTextEx(*font, label.c_str(),
                   {area.x + 39.f, area.y + (area.height - labelSize) * .5f},
                   labelSize, 1.f, RAYWHITE);
        UI::endScissor();
        const Rectangle sameRowButton{area.x + area.width - 75.f, area.y + 6.f, 30.f, 32.f};
        const bool canJoin = index > 0;
        const bool joined = canJoin && block.sameRowWithPrevious;
        DrawRectangleRounded(sameRowButton, .22f, 6,
                             joined ? Color{56, 120, 174, 255} : Color{50, 61, 77, 255});
        const Color joinColor = canJoin ? RAYWHITE : Color{91, 101, 116, 255};
        DrawRectangleRounded({sameRowButton.x + 5.f, sameRowButton.y + 10.f, 7.f, 12.f},
                             .25f, 4, joinColor);
        DrawRectangleRounded({sameRowButton.x + 18.f, sameRowButton.y + 10.f, 7.f, 12.f},
                             .25f, 4, joinColor);
        DrawLineEx({sameRowButton.x + 11.f, sameRowButton.y + 16.f},
                   {sameRowButton.x + 19.f, sameRowButton.y + 16.f}, 2.f, joinColor);
        if (block.removable) {
            const Rectangle remove{area.x + area.width - 39.f, area.y + 6.f, 31.f, 32.f};
            DrawRectangleRounded(remove, .22f, 6, Color{49, 151, 111, 255});
            DrawRectangleRounded({remove.x + 7.f, remove.y + 14.f, remove.width - 14.f, 4.f},
                                 .45f, 5, WHITE);
        }
    }
    UI::endScissor();
    drawScrollbar(false);

    const Rectangle availableViewport = columnViewport(true);
    UI::beginScissor(availableViewport);
    for (std::size_t index = 0; index < m_placeholders.size(); ++index) {
        const auto& placeholder = m_placeholders[index];
        const Rectangle area = rowArea(true, index);
        if (area.y + area.height < availableViewport.y || area.y > availableViewport.y + availableViewport.height)
            continue;
        DrawRectangleRounded(area, .10f, 6, Color{44, 61, 80, 255});
        DrawRectangleRoundedLinesEx(area, .10f, 6, 1.f, Color{82, 109, 139, 255});
        UI::beginScissor({area.x + 8.f, area.y, std::max(0.f, area.width - 44.f), area.height});
        const float labelSize = static_cast<float>(fontSize - 2);
        const std::string label = localizedLabel(placeholder);
        DrawTextEx(*font, label.c_str(),
                   {area.x + 9.f, area.y + (area.height - labelSize) * .5f},
                   labelSize, 1.f, RAYWHITE);
        UI::endScissor();
        const float plusX = area.x + area.width - 24.f;
        DrawRectangleRounded({plusX - 2.f, area.y + 14.f, 4.f, 22.f}, .45f, 5, Color{105, 220, 174, 255});
        DrawRectangleRounded({plusX - 11.f, area.y + 23.f, 22.f, 4.f}, .45f, 5, Color{105, 220, 174, 255});
    }
    UI::endScissor();
    drawScrollbar(true);
    drawPreview();
}
