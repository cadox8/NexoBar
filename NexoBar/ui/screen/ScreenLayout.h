#pragma once

#include "IScreen.h"
#include "../components/layout/Layout.h"
#include <string>
#include <utility>
#include <vector>

// A responsive region inside a ScreenLayout. The normalized rectangle is
// relative to the parent's area; pixel insets are useful for gutters/headers.
struct ScreenRegion {
    Rectangle relative{0.f, 0.f, 1.f, 1.f};
    float left = 0.f, top = 0.f, right = 0.f, bottom = 0.f;
};

class ScreenLayout : public IScreen {
public:
    using ScreenEntry = std::pair<std::string, IScreenPtr>;

    explicit ScreenLayout(LayoutAxis axis = LayoutAxis::Vertical) : m_flow(axis) {}

    ScreenLayout& direction(LayoutAxis axis) {
        m_flow.axis(axis);
        return *this;
    }

    ScreenLayout& gap(float value) {
        m_flow.gap(value);
        return *this;
    }

    ScreenLayout& padding(LayoutInsets value) {
        m_flow.padding(value);
        return *this;
    }

    ScreenLayout& alignment(LayoutAlignment value) {
        m_flow.alignment(value);
        return *this;
    }

    ScreenLayout& justification(LayoutJustification value) {
        m_flow.justification(value);
        return *this;
    }

    ScreenLayout& grid(std::vector<float> rows, std::vector<float> columns);

    void addScreen(std::string name, const IScreenPtr& screen, LayoutOptions options = {});

    void addWeighted(std::string name, const IScreenPtr& screen, float weight = 1.f);

    void addScreenAt(std::string name, const IScreenPtr& screen, int row, int column,
                     int rowSpan = 1, int columnSpan = 1, LayoutOptions options = {});

    void addRegion(std::string name, IScreenPtr screen, ScreenRegion region = {});

    void setScreen(std::string name, const IScreenPtr& screen);

    void setRegion(std::string name, const IScreenPtr& screen, ScreenRegion region);

    void setScreens(const std::vector<ScreenEntry>& screens);

    bool removeScreen(const std::string& name);

    [[nodiscard]] IScreenPtr screen(const std::string& name) const;

    [[nodiscard]] ScreenRegion region(const std::string& name) const;

    void area(const Rectangle& value) override;

    void update(float deltaSeconds) override;

    void draw() override;

private:
    class ScreenAdapter;

    struct Entry {
        std::string name;
        IScreenPtr screen;
        std::shared_ptr<ScreenAdapter> adapter;
        ScreenRegion region{};
        LayoutOptions options{};
        int row = -1, column = -1, rowSpan = 1, columnSpan = 1;
    };

    enum class Mode { Empty, Flow, Absolute };

    void requireMode(Mode requested);

    void rebuildFlow();

    [[nodiscard]] Rectangle resolve(const ScreenRegion& region) const noexcept;

    std::vector<Entry> m_entries;
    Layout m_flow;
    Mode m_mode = Mode::Empty;
    bool m_gridConfigured = false;
};
