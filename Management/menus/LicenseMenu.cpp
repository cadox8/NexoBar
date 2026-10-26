#include "LicenseMenu.h"

#include "../Management.h"
#include "license/license.h"

#include <vector>

void LicenseMenu::init() {
    ManagementImp management;
    const auto version = "Version: " + management->version().version();
    beginMenu("License management", "Back", 70);
    setHeaderLines([this, version] {
        return std::vector<HeaderLine>{
            {"NexoBar Management", ftxui::Color::Cyan},
            {"License management", ftxui::Color::BlueLight},
            {version, ftxui::Color::LightCoral}
        };
    });

    addChoiceAction<License::LicenseType>(
        "Generate license",
        "Select license type",
        {
            {"Basic", License::LicenseType::Basic},
            {"Standard", License::LicenseType::Standard},
            {"Professional", License::LicenseType::Pro},
            {"Enterprise", License::LicenseType::Enterprise}
        },
        [this](const License::LicenseType& type) {
            License license;
            const std::string generated = license.generate(type);
            if (generated.empty()) return std::string("Could not generate the license.");
            return generated;
        });

    addPromptAction<std::string>(
        "Validate license",
        "Paste the license here",
        parseText,
        [this](const std::string& licenseText) {
            if (licenseText.empty()) return std::string("The license is empty.");
            License license;
            const License::LicenseType type = license.validate(licenseText);
            if (type == License::LicenseType::Invalid) return std::string("Invalid license.");

            std::string typeKey;
            switch (type) {
                case License::LicenseType::Basic: typeKey = "Basic"; break;
                case License::LicenseType::Standard: typeKey = "Standard"; break;
                case License::LicenseType::Pro: typeKey = "Professional"; break;
                case License::LicenseType::Enterprise: typeKey = "Enterprise"; break;
                default: typeKey = "Invalid"; break;
            }
            return "Valid license: " + typeKey;
        });

    setFooterAction("Back", [this] {
        if (on_back) on_back();
    });
}
