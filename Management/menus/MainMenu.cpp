#include "MainMenu.h"

#include "../Management.h"

#include <sstream>

void MainMenu::setVersionColor(const ftxui::Color color) {
    version_color_ = color;
}

void MainMenu::init() {
    ManagementImp management;
    std::stringstream versionText;
    versionText << "Version: " << management->version().version();

    beginMenu("NexoBar Management", "Exit", 65);
    setSelectionColor(ftxui::Color::BlueLight);
    setHeaderLines([this, version = versionText.str()] {
        return std::vector<HeaderLine>{
            {"NexoBar Management", ftxui::Color::Cyan},
            {version, version_color_}
        };
    });
    addAction("License management", [this] {
        if (on_open_licenses)
            on_open_licenses();
    });
    addAction("Register a business", [this] {
        if (on_register_business) on_register_business();
    });
    addAction("Simulate a business", [this] {
        if (on_simulate_business) on_simulate_business();
    });
    setFooterAction("Exit", [this] {
        if (on_exit)
            on_exit();
    });
}
