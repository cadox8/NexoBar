#include "ScreenLayout.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

class ScreenLayout::ScreenAdapter final : public IComponent {
public:
    explicit ScreenAdapter(IScreenPtr value) : screen(std::move(value)) {}

    void update(float dt) override {
        screen->area(m_area);
        screen->update(dt);
    }

    void draw() override {
        screen->area(m_area);
        screen->draw();
    }

    IScreenPtr screen;
};

namespace {
    void validateRegion(const ScreenRegion& r) {
        const float values[] = {
            r.relative.x, r.relative.y, r.relative.width, r.relative.height,
            r.left, r.top, r.right, r.bottom
        };
        for (float value: values)
            if (!std::isfinite(value)) throw std::invalid_argument("Screen region values must be finite");
        if (r.relative.width < 0 || r.relative.height < 0)
            throw std::invalid_argument("Screen region size cannot be negative");
    }
}

void ScreenLayout::requireMode(const Mode requested) {
    if (m_mode != Mode::Empty && m_mode != requested)
        throw std::logic_error("A ScreenLayout cannot mix flex/grid children with absolute regions");
    m_mode = requested;
}

ScreenLayout& ScreenLayout::grid(std::vector<float> rows, std::vector<float> columns) {
    if (!m_entries.empty()) throw std::logic_error("Configure the screen grid before adding screens");
    m_flow.grid(std::move(rows), std::move(columns));
    m_gridConfigured = true;
    return *this;
}

void ScreenLayout::addScreen(std::string name, const IScreenPtr& child, LayoutOptions options) {
    requireMode(Mode::Flow);
    if (name.empty() || !child) throw std::invalid_argument("A screen needs a name and a screen instance");
    if (screen(name)) throw std::invalid_argument("A screen layout cannot contain duplicate names");
    if (m_gridConfigured) {
        // Layout assigns the next free grid cell when add() is used.
        auto adapter = std::make_shared<ScreenAdapter>(child);
        m_entries.push_back({std::move(name), child, adapter, {}, options});
        rebuildFlow();
        return;
    }
    m_entries.push_back({std::move(name), child, std::make_shared<ScreenAdapter>(child), {}, options});
    rebuildFlow();
}

void ScreenLayout::addWeighted(std::string name, const IScreenPtr& child, const float weight) {
    LayoutOptions options;
    if (m_flow.direction() == LayoutAxis::Horizontal) options.width = LayoutSize::fill(weight);
    else options.height = LayoutSize::fill(weight);
    addScreen(std::move(name), child, options);
}

void ScreenLayout::addScreenAt(std::string name, const IScreenPtr& child, int row, int column,
                               int rowSpan, int columnSpan, LayoutOptions options) {
    requireMode(Mode::Flow);
    if (!m_gridConfigured) throw std::logic_error("Configure the screen grid before addScreenAt");
    if (name.empty() || !child) throw std::invalid_argument("A screen needs a name and a screen instance");
    if (screen(name)) throw std::invalid_argument("A screen layout cannot contain duplicate names");
    m_entries.push_back({
        std::move(name), child, std::make_shared<ScreenAdapter>(child), {}, options,
        row, column, rowSpan, columnSpan
    });
    rebuildFlow();
}

void ScreenLayout::addRegion(std::string name, IScreenPtr child, ScreenRegion bounds) {
    requireMode(Mode::Absolute);
    if (name.empty() || !child) throw std::invalid_argument("A screen region needs a name and a screen");
    if (screen(name)) throw std::invalid_argument("A screen layout cannot contain duplicate names");
    validateRegion(bounds);
    m_entries.push_back({std::move(name), std::move(child), nullptr, bounds});
    m_entries.back().screen->area(resolve(bounds));
}

void ScreenLayout::setScreen(std::string name, const IScreenPtr& child) {
    if (m_mode == Mode::Flow) {
        if (name.empty() || !child) throw std::invalid_argument("A screen needs a name and a screen instance");
        const auto it = std::ranges::find_if(m_entries, [&name](const Entry& e) { return e.name == name; });
        if (it == m_entries.end()) addScreen(std::move(name), child);
        else {
            it->screen = child;
            it->adapter = std::make_shared<ScreenAdapter>(child);
            rebuildFlow();
        }
        return;
    }
    setRegion(std::move(name), child, {});
}

void ScreenLayout::setRegion(std::string name, const IScreenPtr& child, ScreenRegion bounds) {
    requireMode(Mode::Absolute);
    if (name.empty() || !child) throw std::invalid_argument("A screen region needs a name and a screen");
    validateRegion(bounds);
    const auto it = std::ranges::find_if(m_entries, [&name](const Entry& e) { return e.name == name; });
    if (it == m_entries.end()) {
        m_entries.push_back({std::move(name), child, nullptr, bounds});
        child->area(resolve(bounds));
    } else {
        it->screen = child;
        it->region = bounds;
        child->area(resolve(bounds));
    }
}

void ScreenLayout::setScreens(const std::vector<ScreenEntry>& screens) {
    m_entries.clear();
    m_flow.clear();
    m_mode = screens.empty() ? Mode::Empty : Mode::Flow;
    for (const auto& [name, child]: screens) {
        if (name.empty() || !child) throw std::invalid_argument("A screen layout entry needs a name and a screen");
        if (screen(name)) throw std::invalid_argument("A screen layout cannot contain duplicate names");
        m_entries.push_back({name, child, std::make_shared<ScreenAdapter>(child)});
    }
    rebuildFlow();
}

bool ScreenLayout::removeScreen(const std::string& name) {
    const auto old = m_entries.size();
    std::erase_if(m_entries, [&name](const Entry& e) { return e.name == name; });
    if (m_entries.size() == old) return false;
    if (m_entries.empty()) m_mode = Mode::Empty;
    else if (m_mode == Mode::Flow) rebuildFlow();
    return true;
}

IScreenPtr ScreenLayout::screen(const std::string& name) const {
    const auto it = std::ranges::find_if(m_entries, [&name](const Entry& e) { return e.name == name; });
    return it == m_entries.end() ? nullptr : it->screen;
}

ScreenRegion ScreenLayout::region(const std::string& name) const {
    const auto it = std::ranges::find_if(m_entries, [&name](const Entry& e) { return e.name == name; });
    return it == m_entries.end() ? ScreenRegion{} : it->region;
}

void ScreenLayout::area(const Rectangle& value) {
    IScreen::area(value);
    if (m_mode == Mode::Flow) m_flow.setArea(value);
    else for (auto& entry: m_entries) entry.screen->area(resolve(entry.region));
}

Rectangle ScreenLayout::resolve(const ScreenRegion& r) const noexcept {
    return {
        m_area.x + m_area.width * r.relative.x + r.left,
        m_area.y + m_area.height * r.relative.y + r.top,
        std::max(0.f, m_area.width * r.relative.width - r.left - r.right),
        std::max(0.f, m_area.height * r.relative.height - r.top - r.bottom)
    };
}

void ScreenLayout::rebuildFlow() {
    if (m_mode != Mode::Flow) return;
    m_flow.clear();
    for (const auto& entry: m_entries) {
        if (m_gridConfigured&& entry.row >= 0)
        m_flow.addAt(entry.adapter, entry.row, entry.column, entry.rowSpan, entry.columnSpan, entry.options);
        else m_flow.add(entry.adapter, entry.options);
    }
    m_flow.setArea(m_area);
}

void ScreenLayout::update(float dt) {
    if (m_mode == Mode::Flow) {
        m_flow.setArea(m_area);
        m_flow.update(dt);
        return;
    }
    const auto snapshot = m_entries;
    for (const auto& entry: snapshot) {
        const auto current = std::ranges::find_if(m_entries, [&](const Entry& e) { return e.name == entry.name; });
        if (current != m_entries.end() && current->screen == entry.screen) {
            entry.screen->area(resolve(current->region));
            entry.screen->update(dt);
        }
    }
}

void ScreenLayout::draw() {
    if (m_mode == Mode::Flow) {
        m_flow.setArea(m_area);
        m_flow.draw();
        return;
    }
    const auto snapshot = m_entries;
    for (const auto& entry: snapshot) {
        const auto current = std::ranges::find_if(m_entries, [&](const Entry& e) { return e.name == entry.name; });
        if (current != m_entries.end() && current->screen == entry.screen) {
            entry.screen->area(resolve(current->region));
            entry.screen->draw();
        }
    }
}
