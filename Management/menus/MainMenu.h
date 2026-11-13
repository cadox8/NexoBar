#pragma once

#include "MenuBase.h"

class MainMenu final : public MenuBase {
public:
    void init() override;
    void setVersionColor(ftxui::Color color);
    std::function<void()> on_open_licenses;
    std::function<void()> on_register_business;
    std::function<void()> on_simulate_business;
    std::function<void()> on_exit;

private:
    ftxui::Color version_color_ = ftxui::Color::LightCoral;
};
