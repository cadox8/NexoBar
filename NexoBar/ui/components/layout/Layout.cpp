#include "Layout.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

static float resolveSize(LayoutSize size, float available) {
    if (size.unit == LayoutUnit::Pixels)return size.value;
    if (size.unit == LayoutUnit::Percent)return available * size.value;
    return -1;
}

static float clampSize(float v, float lo, float hi) {
    v = std::max(v, lo);
    if (hi > 0)v = std::min(v, hi);
    return v;
}

static void validateOptions(const LayoutOptions& options) {
    const auto validSize = [](LayoutSize size) { return std::isfinite(size.value) && size.value >= 0; };
    if (!validSize(options.width) || !validSize(options.height))
        throw std::invalid_argument("Layout sizes must be finite and non-negative");
    if (!std::isfinite(options.minWidth) || !std::isfinite(options.minHeight) || options.minWidth < 0 || options.minHeight < 0
        || !std::isfinite(options.maxWidth) || !std::isfinite(options.maxHeight) || options.maxWidth < 0 || options.maxHeight < 0
        || (options.maxWidth > 0 && options.maxWidth < options.minWidth)
        || (options.maxHeight > 0 && options.maxHeight < options.minHeight))
        throw std::invalid_argument("Layout minimum and maximum sizes are invalid");
    const LayoutInsets margin = options.margin;
    if (!std::isfinite(margin.left) || !std::isfinite(margin.top) || !std::isfinite(margin.right) || !std::isfinite(margin.bottom))
        throw std::invalid_argument("Layout margins must be finite");
}

static Rectangle inset(Rectangle r, LayoutInsets p) { return {r.x + p.left, r.y + p.top, std::max(0.f, r.width - p.left - p.right), std::max(0.f, r.height - p.top - p.bottom)}; }

static Rectangle fitCell(Rectangle cell, const LayoutOptions& o, LayoutAlignment inherited) {
    Rectangle available = inset(cell, o.margin);
    float w = resolveSize(o.width, available.width), h = resolveSize(o.height, available.height);
    const LayoutAlignment alignment = o.alignment == LayoutAlignment::Inherit ? inherited : o.alignment;
    if (w < 0 || alignment == LayoutAlignment::Stretch)w = available.width;
    if (h < 0 || alignment == LayoutAlignment::Stretch)h = available.height;
    w = clampSize(w, o.minWidth, o.maxWidth);
    h = clampSize(h, o.minHeight, o.maxHeight);
    float x = available.x, y = available.y;
    if (alignment == LayoutAlignment::Center) {
        x += (available.width - w) * .5f;
        y += (available.height - h) * .5f;
    } else if (alignment == LayoutAlignment::End) {
        x += available.width - w;
        y += available.height - h;
    }
    return {x, y, w, h};
}

Layout& Layout::add(ComponentPtr child, LayoutOptions options) {
    if (child) {
        validateOptions(options);
        if (std::any_of(m_children.begin(), m_children.end(), [&](const Child& item) { return item.component == child; }))
            throw std::invalid_argument("A component instance can only be added to a Layout once");
        Child item{std::move(child), options};
        if (!m_rowWeights.empty() && !m_columnWeights.empty()) {
            bool found = false;
            for (int row = 0; row < int(m_rowWeights.size()) && !found; ++row)
                for (int column = 0; column < int(m_columnWeights.size()) && !found; ++column) {
                    bool occupied = false;
                    for (const auto& current: m_children)
                        if (current.row >= 0 && row >= current.row && row < current.row + current.rowSpan && column >= current.column && column < current.column + current.columnSpan) {
                            occupied = true;
                            break;
                        }
                    if (!occupied) {
                        item.row = row;
                        item.column = column;
                        found = true;
                    }
                }
            if (!found)throw std::out_of_range("Layout has no free grid cells");
        }
        m_children.push_back(std::move(item));
    }
    return *this;
}

Layout& Layout::add(ComponentPtr child, LayoutSize width, LayoutSize height, LayoutInsets margin) {
    LayoutOptions o;
    o.width = width;
    o.height = height;
    o.margin = margin;
    return add(std::move(child), o);
}

Layout& Layout::addWeighted(ComponentPtr child, float weight) { return add(std::move(child), m_axis == LayoutAxis::Horizontal ? LayoutSize::fill(weight) : LayoutSize::fill(), m_axis == LayoutAxis::Vertical ? LayoutSize::fill(weight) : LayoutSize::fill()); }
Layout& Layout::addPixels(ComponentPtr child, float width, float height, LayoutInsets margin) { return add(std::move(child), LayoutSize::px(width), LayoutSize::px(height), margin); }
Layout& Layout::addPercent(ComponentPtr child, float width, float height, LayoutInsets margin) { return add(std::move(child), LayoutSize::percent(width), LayoutSize::percent(height), margin); }

Layout& Layout::grid(std::vector<float> rows, std::vector<float> columns) {
    if (rows.empty() || columns.empty())throw std::invalid_argument("Layout grid needs at least one row and one column");
    for (float v: rows)if (!std::isfinite(v) || v < 0)throw std::invalid_argument("Grid row weights must be finite and non-negative");
    for (float v: columns)if (!std::isfinite(v) || v < 0)throw std::invalid_argument("Grid column weights must be finite and non-negative");
    if (std::none_of(rows.begin(), rows.end(), [](float v) { return v > 0; }) || std::none_of(columns.begin(), columns.end(), [](float v) { return v > 0; }))
        throw std::invalid_argument("Grid needs at least one positive row and column weight");
    for (std::size_t i = 0; i < m_children.size(); ++i) {
        const int row = int(i / columns.size());
        if (row >= int(rows.size()))throw std::out_of_range("Existing children exceed the configured grid");
    }
    m_rowWeights = std::move(rows);
    m_columnWeights = std::move(columns);
    for (std::size_t i = 0; i < m_children.size(); ++i) {
        m_children[i].row = int(i / m_columnWeights.size());
        m_children[i].column = int(i % m_columnWeights.size());
        m_children[i].rowSpan = m_children[i].columnSpan = 1;
    }
    return *this;
}

Layout& Layout::addAt(ComponentPtr child, int row, int column, int rowSpan, int columnSpan, LayoutOptions options) {
    if (m_rowWeights.empty() || m_columnWeights.empty())throw std::logic_error("Configure grid rows and columns before addAt");
    if (row < 0 || column < 0 || rowSpan < 1 || columnSpan < 1 || row + rowSpan > int(m_rowWeights.size()) || column + columnSpan > int(m_columnWeights.size()))throw std::out_of_range("Layout grid cell or span is outside the configured grid");
    if (child) {
        validateOptions(options);
        if (std::any_of(m_children.begin(), m_children.end(), [&](const Child& item) { return item.component == child; }))
            throw std::invalid_argument("A component instance can only be added to a Layout once");
        for (const auto& current: m_children) {
            if (current.row < 0)continue;
            const bool intersects = row < current.row + current.rowSpan && row + rowSpan > current.row
                                    && column < current.column + current.columnSpan && column + columnSpan > current.column;
            if (intersects)throw std::invalid_argument("Layout grid cells cannot overlap");
        }
        m_children.push_back({std::move(child), options, row, column, rowSpan, columnSpan});
    }
    return *this;
}

Layout& Layout::insert(std::size_t index, ComponentPtr child, LayoutOptions options) {
    if (index > m_children.size())throw std::out_of_range("Layout insertion index is out of range");
    if (!child)return *this;
    const std::size_t oldSize = m_children.size();
    add(std::move(child), options);
    std::rotate(m_children.begin() + static_cast<std::ptrdiff_t>(index),
                m_children.begin() + static_cast<std::ptrdiff_t>(oldSize), m_children.end());
    return *this;
}

Layout& Layout::replace(std::size_t index, ComponentPtr child, LayoutOptions options) {
    const auto& previous = m_children.at(index).component;
    if (child) {
        validateOptions(options);
        if (std::any_of(m_children.begin(), m_children.end(), [&](const Child& item) { return item.component == child && item.component != previous; }))
            throw std::invalid_argument("A component instance can only be added to a Layout once");
    }
    auto& current = m_children.at(index);
    current.component = std::move(child);
    current.options = options;
    return *this;
}

Layout& Layout::remove(const ComponentPtr& child) {
    std::erase_if(m_children, [&](const Child& item) { return item.component == child; });
    return *this;
}

ComponentPtr Layout::at(std::size_t index) const { return m_children.at(index).component; }

LayoutOptions Layout::optionsAt(std::size_t index) const { return m_children.at(index).options; }

Layout& Layout::clear() {
    m_children.clear();
    return *this;
}

void Layout::setArea(const Rectangle& area) { m_area = area; }

std::vector<float> Layout::resolveTracks(const std::vector<float>& weights, float extent, float gap) const {
    std::vector<float> out;
    out.reserve(weights.size());
    if (weights.empty())return out;
    const float available = std::max(0.f, extent - gap * float(weights.size() - 1));
    float total = 0;
    for (float w: weights)total += w;
    for (float w: weights)out.push_back(total > 0 ? available * w / total : 0);
    return out;
}

float Layout::requiredExtent(float viewport) const {
    const bool horizontal = m_axis == LayoutAxis::Horizontal;
    if (!m_rowWeights.empty() && !m_columnWeights.empty()) {
        const auto& tracks = horizontal ? m_columnWeights : m_rowWeights;
        std::vector<float> required(tracks.size(), 0.f);
        for (const auto& child: m_children) {
            const int begin = horizontal ? child.column : child.row;
            const int span = horizontal ? child.columnSpan : child.rowSpan;
            if (begin < 0 || begin >= int(required.size()))continue;
            const LayoutSize size = horizontal ? child.options.width : child.options.height;
            float extent = resolveSize(size, viewport);
            if (extent < 0)extent = horizontal ? child.options.minWidth : child.options.minHeight;
            if (extent <= 0)extent = std::max(64.f, viewport * .25f);
            extent = clampSize(extent, horizontal ? child.options.minWidth : child.options.minHeight,
                               horizontal ? child.options.maxWidth : child.options.maxHeight);
            extent += horizontal
                          ? child.options.margin.left + child.options.margin.right
                          : child.options.margin.top + child.options.margin.bottom;
            const float perTrack = extent / float(std::max(1, span));
            for (int track = begin; track < std::min(begin + span, int(required.size())); ++track)
                required[std::size_t(track)] = std::max(required[std::size_t(track)], perTrack);
        }
        float total = horizontal ? m_padding.left + m_padding.right : m_padding.top + m_padding.bottom;
        for (float extent: required)total += extent;
        if (required.size() > 1)total += (horizontal ? m_gapX : m_gapY) * float(required.size() - 1);
        return total;
    }
    float total = 0;
    for (const auto& c: m_children) {
        const auto s = horizontal ? c.options.width : c.options.height;
        float v = resolveSize(s, viewport);
        if (v < 0)v = std::max(horizontal ? c.options.minWidth : c.options.minHeight, std::max(64.f, viewport * .25f));
        v = clampSize(v, horizontal ? c.options.minWidth : c.options.minHeight,
                      horizontal ? c.options.maxWidth : c.options.maxHeight);
        total += v + (horizontal ? c.options.margin.left + c.options.margin.right : c.options.margin.top + c.options.margin.bottom);
    }
    if (m_children.size() > 1)total += (horizontal ? m_gapX : m_gapY) * float(m_children.size() - 1);
    return total + (horizontal ? m_padding.left + m_padding.right : m_padding.top + m_padding.bottom);
}

std::vector<Rectangle> Layout::arrange(const Rectangle& area) const {
    std::vector<Rectangle> out(m_children.size());
    Rectangle box{area.x + m_padding.left, area.y + m_padding.top, std::max(0.f, area.width - m_padding.left - m_padding.right), std::max(0.f, area.height - m_padding.top - m_padding.bottom)};
    const bool gridMode = !m_rowWeights.empty() && !m_columnWeights.empty();
    if (gridMode) {
        const auto rows = resolveTracks(m_rowWeights, box.height, m_gapY), cols = resolveTracks(m_columnWeights, box.width, m_gapX);
        std::vector<float> xs(cols.size() + 1, box.x), ys(rows.size() + 1, box.y);
        for (std::size_t i = 0; i < cols.size(); ++i)xs[i + 1] = xs[i] + cols[i] + m_gapX;
        for (std::size_t i = 0; i < rows.size(); ++i)ys[i + 1] = ys[i] + rows[i] + m_gapY;
        for (std::size_t i = 0; i < m_children.size(); ++i) {
            const auto& c = m_children[i];
            if (c.row < 0)continue;
            Rectangle cell{xs[std::size_t(c.column)], ys[std::size_t(c.row)], 0, 0};
            for (int col = c.column; col < c.column + c.columnSpan; ++col)cell.width += cols[std::size_t(col)];
            for (int row = c.row; row < c.row + c.rowSpan; ++row)cell.height += rows[std::size_t(row)];
            cell.width += m_gapX * float(c.columnSpan - 1);
            cell.height += m_gapY * float(c.rowSpan - 1);
            out[i] = fitCell(cell, c.options, m_alignment);
        }
        return out;
    }
    if (m_children.empty())return out;
    const bool horizontal = m_axis == LayoutAxis::Horizontal;
    const float main = horizontal ? box.width : box.height, cross = horizontal ? box.height : box.width;
    float gap = horizontal ? m_gapX : m_gapY;
    const float available = std::max(0.f, main - gap * float(m_children.size() - 1));
    float fixed = 0, weights = 0;
    for (const auto& c: m_children) {
        const auto l = horizontal ? c.options.width : c.options.height;
        const float v = resolveSize(l, available);
        if (v >= 0)fixed += v;
        else weights += std::max(0.f, l.value);
        fixed += horizontal ? c.options.margin.left + c.options.margin.right : c.options.margin.top + c.options.margin.bottom;
    }
    const float flex = std::max(0.f, available - fixed);
    std::vector<float> extents;
    extents.reserve(m_children.size());
    float used = 0;
    for (const auto& c: m_children) {
        const auto l = horizontal ? c.options.width : c.options.height;
        float v = resolveSize(l, available);
        if (v < 0)v = weights > 0 ? flex * std::max(0.f, l.value) / weights : 0;
        v += horizontal ? c.options.margin.left + c.options.margin.right : c.options.margin.top + c.options.margin.bottom;
        extents.push_back(std::max(0.f, v));
        used += extents.back();
    }
    float cursor = horizontal ? box.x : box.y;
    const float free = std::max(0.f, available - used);
    if (m_justification == LayoutJustification::Center)cursor += free * .5f;
    else if (m_justification == LayoutJustification::End)cursor += free;
    else if (m_justification == LayoutJustification::SpaceBetween && m_children.size() > 1)gap += free / float(m_children.size() - 1);
    else if (m_justification == LayoutJustification::SpaceAround) {
        gap += free / float(m_children.size());
        cursor += free / (2 * float(m_children.size()));
    }
    for (std::size_t i = 0; i < m_children.size(); ++i) {
        const auto& o = m_children[i].options;
        const auto p = o.margin;
        const auto crossLength = horizontal ? o.height : o.width;
        const float crossAvailable = cross - (horizontal ? p.top + p.bottom : p.left + p.right);
        float crossExtent = resolveSize(crossLength, crossAvailable);
        LayoutAlignment align = o.alignment == LayoutAlignment::Inherit ? m_alignment : o.alignment;
        if (crossExtent < 0 || align == LayoutAlignment::Stretch)crossExtent = crossAvailable;
        crossExtent = clampSize(crossExtent, horizontal ? o.minHeight : o.minWidth, horizontal ? o.maxHeight : o.maxWidth);
        float crossCursor = horizontal ? box.y + p.top : box.x + p.left;
        if (align == LayoutAlignment::Center)crossCursor += (crossAvailable - crossExtent) * .5f;
        else if (align == LayoutAlignment::End)crossCursor += crossAvailable - crossExtent;
        const float mainStart = cursor + (horizontal ? p.left : p.top);
        const float mainExtent = std::max(0.f, extents[i] - (horizontal ? p.left + p.right : p.top + p.bottom));
        out[i] = horizontal ? Rectangle{mainStart, crossCursor, clampSize(mainExtent, o.minWidth, o.maxWidth), crossExtent} : Rectangle{crossCursor, mainStart, crossExtent, clampSize(mainExtent, o.minHeight, o.maxHeight)};
        cursor += extents[i] + gap;
    }
    return out;
}

void Layout::update(float dt) {
    const auto areas = arrange(m_area);
    const auto snapshot = m_children;
    for (std::size_t i = 0; i < snapshot.size(); ++i) {
        const auto& c = snapshot[i];
        if (!c.component || i >= areas.size())continue;
        const bool stillPresent = std::any_of(m_children.begin(), m_children.end(), [&](const Child& current) { return current.component == c.component; });
        if (!stillPresent)continue;
        c.component->area(areas[i]);
        if (c.component->visible)c.component->update(dt);
    }
}

void Layout::draw() {
    const auto areas = arrange(m_area);
    for (std::size_t i = 0; i < m_children.size(); ++i) {
        const auto& c = m_children[i];
        if (!c.component || !c.component->visible)continue;
        if (debug)DrawRectangleLinesEx(areas[i], 1, Color{100, 120, 145, 180});
        c.component->draw();
    }
}
