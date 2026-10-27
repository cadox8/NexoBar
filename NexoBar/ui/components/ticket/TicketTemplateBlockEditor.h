#pragma once

#include "../core/IComponent.h"
#include "../../../manager/resources/ResourceTypes.h"

#include <string>
#include <vector>

class TicketTemplateBlockEditor final : public IComponent {
public:
    struct Block {
        std::string id;
        std::string label;
        std::string token;
        std::string source;
        bool removable = true;
        bool sameRowWithPrevious = false;
    };

    TicketTemplateBlockEditor();

    FontPtr font;
    int fontSize = 17;

    void load(const std::string& source);
    [[nodiscard]] std::string templateText() const;
    [[nodiscard]] const std::vector<Block>& blocks() const noexcept { return m_blocks; }
    void update(float deltaSeconds) override;
    void draw() override;

private:
    [[nodiscard]] Rectangle editorArea() const noexcept;
    [[nodiscard]] Rectangle previewArea() const noexcept;
    [[nodiscard]] Rectangle columnHeader(bool available) const noexcept;
    [[nodiscard]] Rectangle columnViewport(bool available) const noexcept;
    [[nodiscard]] Rectangle rowArea(bool available, std::size_t index) const noexcept;
    [[nodiscard]] float maxScroll(bool available) const noexcept;
    void clampScroll();
    void drawHeader(bool available) const;
    void drawScrollbar(bool available) const;
    void drawPreview() const;

    std::vector<Block> m_blocks;
    std::vector<Block> m_placeholders;
    std::size_t m_serial = 0;
    float m_blockScroll = 0.f;
    float m_placeholderScroll = 0.f;
    float m_dragLastY = 0.f;
    float m_dragDistance = 0.f;
    int m_dragColumn = -1;
};

using TicketTemplateBlockEditorPtr = std::shared_ptr<TicketTemplateBlockEditor>;
