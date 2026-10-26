#pragma once
#include "../core/IComponent.h"
#include <algorithm>
#include <cstddef>
#include <vector>

enum class LayoutAxis { Horizontal, Vertical };

enum class LayoutAlignment { Inherit, Start, Center, End, Stretch };

enum class LayoutJustification { Start, Center, End, SpaceBetween, SpaceAround };

enum class LayoutUnit { Fill, Pixels, Percent };

struct LayoutSize {
    LayoutUnit unit = LayoutUnit::Fill;
    float value = 1;
    static LayoutSize fill(float weight = 1) { return {LayoutUnit::Fill, weight}; }
    static LayoutSize px(float value) { return {LayoutUnit::Pixels, value}; }
    static LayoutSize percent(float value) { return {LayoutUnit::Percent, value}; }
};

struct LayoutInsets {
    float left = 0, top = 0, right = 0, bottom = 0;
    static LayoutInsets all(float v) { return {v, v, v, v}; }
    static LayoutInsets symmetric(float horizontal, float vertical) { return {horizontal, vertical, horizontal, vertical}; }
};

struct LayoutOptions {
    LayoutSize width = LayoutSize::fill();
    LayoutSize height = LayoutSize::fill();
    LayoutInsets margin{};
    float minWidth = 0, minHeight = 0;
    float maxWidth = 0, maxHeight = 0; // zero means unbounded
    LayoutAlignment alignment = LayoutAlignment::Inherit;
};

class Layout {
public:
    explicit Layout(LayoutAxis axis = LayoutAxis::Vertical) : m_axis(axis) {}

    Layout& axis(LayoutAxis value) {
        m_axis = value;
        return *this;
    }

    Layout& gap(float value) {
        m_gapX = std::max(0.f, value);
        m_gapY = std::max(0.f, value);
        return *this;
    }

    Layout& gap(float horizontal, float vertical) {
        m_gapX = std::max(0.f, horizontal);
        m_gapY = std::max(0.f, vertical);
        return *this;
    }

    Layout& padding(LayoutInsets value) {
        m_padding = value;
        return *this;
    }

    Layout& alignment(LayoutAlignment value) {
        m_alignment = value;
        return *this;
    }

    Layout& justification(LayoutJustification value) {
        m_justification = value;
        return *this;
    }

    Layout& add(ComponentPtr child, LayoutOptions options = {});

    Layout& add(ComponentPtr child, LayoutSize width, LayoutSize height, LayoutInsets margin = {});

    Layout& addWeighted(ComponentPtr child, float weight);

    Layout& addPixels(ComponentPtr child, float width, float height, LayoutInsets margin = {});

    Layout& addPercent(ComponentPtr child, float width, float height, LayoutInsets margin = {});

    Layout& grid(std::vector<float> rowWeights, std::vector<float> columnWeights);

    Layout& addAt(ComponentPtr child, int row, int column, int rowSpan = 1, int columnSpan = 1, LayoutOptions options = {});

    Layout& insert(std::size_t index, ComponentPtr child, LayoutOptions options = {});

    Layout& replace(std::size_t index, ComponentPtr child, LayoutOptions options = {});

    Layout& remove(const ComponentPtr& child);

    ComponentPtr at(std::size_t index) const;

    LayoutOptions optionsAt(std::size_t index) const;

    Layout& clear();

    std::size_t size() const noexcept { return m_children.size(); }

    void update(float deltaSeconds);

    void draw();

    void setArea(const Rectangle& area);

    std::vector<Rectangle> arrange(const Rectangle& area) const;

    float requiredExtent(float viewport) const;

    LayoutAxis direction() const noexcept { return m_axis; }
    bool debug = false;

private:
    struct Child {
        ComponentPtr component;
        LayoutOptions options;
        int row = -1, column = -1, rowSpan = 1, columnSpan = 1;
    };

    std::vector<float> resolveTracks(const std::vector<float>& weights, float extent, float gap) const;

    LayoutAxis m_axis;
    LayoutAlignment m_alignment = LayoutAlignment::Stretch;
    LayoutJustification m_justification = LayoutJustification::Start;
    LayoutInsets m_padding{};
    float m_gapX = 0, m_gapY = 0;
    Rectangle m_area{};
    std::vector<Child> m_children;
    std::vector<float> m_rowWeights, m_columnWeights;
};

using LayoutPtr = std::shared_ptr<Layout>;
