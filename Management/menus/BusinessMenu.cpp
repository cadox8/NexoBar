#include "BusinessMenu.h"

#include "license/license.h"

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <cctype>
#include <optional>

#include "database/models/Category.h"
#include "database/models/Product.h"

namespace {
    std::string trim(std::string value) {
        const auto isSpace = [](const unsigned char c) { return std::isspace(c) != 0; };
        value.erase(value.begin(), std::find_if_not(value.begin(), value.end(), isSpace));
        value.erase(std::find_if_not(value.rbegin(), value.rend(), isSpace).base(), value.end());
        return value;
    }
}

BusinessMenu::BusinessMenu(Nearon::Database& database)
    : database_(database), labels_{
          {
              "Legal name *", "Trading name *", "Tax ID (NIF) *", "Registered address *",
              "Postal code *", "City *", "Province *", "Country (ISO 3166-1, e.g. ES)",
              "Phone (optional)", "Email (optional)", "License *"
          }
      } {}

void BusinessMenu::loadExisting() {
    values_.fill({});
    const auto stored = database_.query<nexobar::Business>().first();
    if (!stored) return;
    values_ = {
        stored->legalName, stored->tradeName, stored->nif, stored->address, stored->postalCode,
        stored->city, stored->province, stored->country,
        stored->phone.value_or(""), stored->email.value_or(""), stored->license
    };
}

void BusinessMenu::refresh() {
    loadExisting();
    feedback_.clear();
}

void BusinessMenu::init() {
    loadExisting();
    inputs_.clear();
    for (std::string& value: values_) inputs_.push_back(ftxui::Input(&value, ""));

    fieldCells_.clear();
    std::vector<ftxui::Component> rows;
    for (std::size_t i = 0; i < inputs_.size(); ++i) {
        auto input = inputs_[i];
        auto label = labels_[i];
        auto cell = ftxui::Renderer(input, [input, label] {
            return ftxui::vbox({
                       ftxui::text(label) | ftxui::bold | ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, 1),
                       input->Render() | ftxui::border | ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, 3)
                   }) | ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, 4)
                   | ftxui::size(ftxui::WIDTH, ftxui::GREATER_THAN, 36) | ftxui::flex;
        });
        cell = ftxui::CatchEvent(cell, [input](ftxui::Event event) {
            if (event.is_mouse()) input->TakeFocus();
            return false;
        });
        fieldCells_.push_back(cell);
    }
    for (std::size_t i = 0; i < fieldCells_.size(); i += 2) {
        if (i + 1 < fieldCells_.size())
            rows.push_back(ftxui::Container::Horizontal({fieldCells_[i], fieldCells_[i + 1]}));
        else
            rows.push_back(fieldCells_[i]);
    }
    auto fields = ftxui::Container::Vertical(rows);
    auto saveButton = ftxui::Button("Save", [this] { save(); });
    auto backButton = ftxui::Button("Back", [this] {
        if (on_back) on_back();
    });
    auto buttons = ftxui::Container::Horizontal({saveButton, backButton});
    auto content = ftxui::Container::Vertical({fields, buttons});
    auto focusOrder = inputs_;
    focusOrder.push_back(saveButton);
    focusOrder.push_back(backButton);

    component_ = ftxui::Renderer(content, [this, fields, saveButton, backButton] {
        return ftxui::vbox({
                   ftxui::text("Register Business") | ftxui::bold | ftxui::color(ftxui::Color::Cyan) | ftxui::center,
                   ftxui::text("Enter the business details. Optional fields may be left blank. Paste the license generated beforehand.") | ftxui::dim,
                   ftxui::separator(),
                   fields->Render() | ftxui::flex,
                   ftxui::paragraph(feedback_) | ftxui::color(feedbackColor_),
                   ftxui::separator(),
                   ftxui::hbox({saveButton->Render(), ftxui::text("  "), backButton->Render()}) | ftxui::center,
                   ftxui::text("Tab/Shift+Tab: change field | Click a field to edit | Enter: activate button") | ftxui::dim | ftxui::center
               }) | ftxui::border | ftxui::size(ftxui::WIDTH, ftxui::LESS_THAN, 100)
               | ftxui::size(ftxui::HEIGHT, ftxui::LESS_THAN, 45) | ftxui::center;
    });
    component_ = ftxui::CatchEvent(component_, [focusOrder](ftxui::Event event) {
        if (event != ftxui::Event::Tab && event != ftxui::Event::TabReverse) return false;
        if (focusOrder.empty()) return true;
        std::size_t current = focusOrder.size() - 1;
        for (std::size_t i = 0; i < focusOrder.size(); ++i) {
            if (focusOrder[i]->Focused()) {
                current = i;
                break;
            }
        }
        const std::size_t next = event == ftxui::Event::Tab
                                     ? (current + 1) % focusOrder.size()
                                     : (current + focusOrder.size() - 1) % focusOrder.size();
        focusOrder[next]->TakeFocus();
        return true;
    });
}

void BusinessMenu::save() {
    auto values = values_;
    for (std::string& value: values) value = trim(std::move(value));

    const std::array required{0u, 1u, 2u, 3u, 4u, 5u, 6u, 10u};
    for (const std::size_t index: required) {
        if (values[index].empty()) {
            feedback_ = "Required field: " + labels_[index];
            feedbackColor_ = ftxui::Color::Red;
            return;
        }
    }

    License license;
    const License::LicenseType licenseType = license.validate(values[10]);
    if (licenseType == License::LicenseType::Invalid) {
        feedback_ = "The license is invalid.";
        feedbackColor_ = ftxui::Color::Red;
        return;
    }

    try {
        auto existing = database_.query<nexobar::Business>().first();
        nexobar::Business business = existing.value_or(nexobar::Business{});
        business.legalName = values[0];
        business.tradeName = values[1];
        business.nif = values[2];
        business.address = values[3];
        business.postalCode = values[4];
        business.city = values[5];
        business.province = values[6];
        business.country = values[7].empty() ? "ES" : values[7];
        business.phone = values[8].empty() ? std::nullopt : std::optional<std::string>(values[8]);
        business.email = values[9].empty() ? std::nullopt : std::optional<std::string>(values[9]);
        business.license = values[10];

        database_.transaction([&] {
            if (existing) database_.update<nexobar::Business>(business);
            else database_.insert<nexobar::Business>(business);
        });
        feedback_ = existing ? "Business details updated." : "Business registered.";
        feedbackColor_ = ftxui::Color::GreenYellow;
    } catch (...) {
        feedback_ = "Could not save the business details.";
        feedbackColor_ = ftxui::Color::Red;
    }
}
