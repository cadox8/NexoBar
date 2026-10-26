#pragma once

#include "MenuBase.h"

class MainMenu final : public MenuBase {
public:
    void init() override;
    void setVersionColor(ftxui::Color color);
    std::function<void()> on_register_business;

private:
    ftxui::Color version_color_ = ftxui::Color::LightCoral;
};
