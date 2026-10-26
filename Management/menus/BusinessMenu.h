#pragma once

#include "database/Database.h"
#include "database/models/Business.h"

#include <ftxui/component/component.hpp>

#include <array>
#include <functional>
#include <memory>
#include <string>
#include <vector>

class BusinessMenu {
public:
    explicit BusinessMenu(Nearon::Database& database);

    void init();
    void refresh();
    [[nodiscard]] ftxui::Component getComponent() const { return component_; }

    std::function<void()> on_back;

private:
    void loadExisting();
    void save();
    Nearon::Database& database_;
    std::array<std::string, 11> values_;
    std::array<std::string, 11> labels_;
    std::vector<ftxui::Component> inputs_;
    std::vector<ftxui::Component> fieldCells_;
    ftxui::Component component_;
    std::string feedback_;
    ftxui::Color feedbackColor_ = ftxui::Color::Red;
};
