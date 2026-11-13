#pragma once

#include <ftxui/component/component.hpp>
#include <ftxui/screen/color.hpp>

#include <charconv>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

class MenuBase {
public:
    virtual ~MenuBase() = default;

    virtual void init() = 0;
    virtual ftxui::Component getComponent();

    std::function<void()> on_back;
    std::function<void()> on_exit;
    std::function<void()> on_open_licenses;
    std::function<void()> on_register_business;

    struct HeaderLine {
        std::string text;
        ftxui::Color color;
    };

    template<typename T>
    void addPromptAction(
        std::string label,
        std::string prompt,
        std::function<std::optional<T>(const std::string&)> parse,
        std::function<std::string(const T&)> createResult) {
        actions_.push_back({
            std::move(label),
            [this, prompt = std::move(prompt), parse = std::move(parse),
             createResult = std::move(createResult)] {
                prompt_ = prompt;
                input_.clear();
                feedback_.clear();
                submit_ = [this, parse, createResult] {
                    const auto value = parse(input_);
                    if (!value) {
                        feedback_ = invalidInputMessage();
                        return;
                    }
                    showResult(createResult(*value));
                };
                switchPage(Page::Prompt);
            }
        });
    }

    template<typename T>
    void addChoiceAction(
        std::string label,
        std::string prompt,
        std::vector<std::pair<std::string, T>> choices,
        std::function<std::string(const T&)> createResult) {
        actions_.push_back({
            std::move(label),
            [this, prompt = std::move(prompt), choices = std::move(choices),
             createResult = std::move(createResult)] {
                if (choices.empty()) {
                    showResult(noOptionsMessage());
                    return;
                }
                prompt_ = prompt;
                feedback_.clear();
                choice_labels_.clear();
                choice_actions_.clear();
                for (const auto& [choiceLabel, value] : choices) {
                    choice_labels_.push_back(choiceLabel);
                    choice_actions_.push_back([this, value, createResult] {
                        showResult(createResult(value));
                    });
                }
                selected_choice_ = 0;
                switchPage(Page::Choice);
            }
        });
    }

    static std::optional<std::string> parseText(const std::string& value) {
        return value;
    }

    template<typename T>
    static std::optional<T> parseInteger(const std::string& value) {
        static_assert(std::is_integral_v<T>, "T must be an integer type");
        T parsed{};
        const auto [end, error] = std::from_chars(
            value.data(), value.data() + value.size(), parsed);
        if (error != std::errc{} || end != value.data() + value.size()) {
            return std::nullopt;
        }
        return parsed;
    }

protected:
    void beginMenu(std::string title, std::string footerLabel, int width = 65);
    void addAction(std::string label, std::function<void()> action);
    void setFooterAction(std::string label, std::function<void()> action);
    void setHeaderLines(std::function<std::vector<HeaderLine>()> headerLines);
    void setSelectionColor(ftxui::Color color);
    ftxui::Color selectionColor() const;
    void showResult(std::string result);
    static const std::string& invalidInputMessage();
    static const std::string& noOptionsMessage();

private:
    enum class Page {
        Menu,
        Prompt,
        Choice,
        Result
    };

    struct Action {
        std::string label;
        std::function<void()> activate;
    };

    void returnToMenu();
    void switchPage(Page page);
    bool copyResult();

    std::vector<Action> actions_;
    std::vector<std::string> action_labels_;
    std::vector<std::string> footer_labels_;
    std::vector<std::string> choice_labels_;
    std::vector<std::function<void()>> choice_actions_;
    std::string title_;
    std::string footer_label_;
    std::string prompt_;
    std::string input_;
    std::string result_;
    std::string feedback_;
    std::function<void()> footer_action_;
    std::function<void()> submit_;
    std::function<std::vector<HeaderLine>()> header_lines_;
    ftxui::Color selection_color_ = ftxui::Color::BlueLight;
    int width_ = 65;
    int selected_action_ = 0;
    int selected_footer_ = 0;
    int selected_choice_ = 0;
    int page_index_ = 0;
    bool action_focused_ = false;
    bool footer_focused_ = false;
    Page page_ = Page::Menu;
};
