#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#ifdef NOCLIPBOARD
#undef NOCLIPBOARD
#endif
#include <windows.h>
#endif

#include "MenuBase.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include <limits>
#include <sstream>
#include <utility>

using namespace ftxui;

const std::string& MenuBase::invalidInputMessage() {
    static const std::string value = "Invalid input.";
    return value;
}

const std::string& MenuBase::noOptionsMessage() {
    static const std::string value = "No options are available.";
    return value;
}

void MenuBase::beginMenu(std::string title, std::string footerLabel, int width) {
    title_ = std::move(title);
    footer_label_ = std::move(footerLabel);
    footer_action_ = {};
    header_lines_ = {};
    width_ = width;
    selection_color_ = Color::BlueLight;
    actions_.clear();
    action_labels_.clear();
    footer_labels_.clear();
    selected_action_ = 0;
    selected_footer_ = 0;
    selected_choice_ = 0;
    action_focused_ = false;
    footer_focused_ = false;
    input_.clear();
    result_.clear();
    feedback_.clear();
    switchPage(Page::Menu);
}

void MenuBase::addAction(std::string label, std::function<void()> action) {
    actions_.push_back({std::move(label), std::move(action)});
}

void MenuBase::setFooterAction(std::string label, std::function<void()> action) {
    footer_label_ = std::move(label);
    footer_action_ = std::move(action);
    footer_labels_ = {footer_label_};
    selected_footer_ = 0;
}

void MenuBase::setHeaderLines(
    std::function<std::vector<HeaderLine>()> headerLines) {
    header_lines_ = std::move(headerLines);
}

void MenuBase::setSelectionColor(const Color color) {
    selection_color_ = color;
}

Color MenuBase::selectionColor() const {
    return selection_color_;
}

void MenuBase::showResult(std::string result) {
    result_ = std::move(result);
    feedback_.clear();
    switchPage(Page::Result);
}

void MenuBase::returnToMenu() {
    switchPage(Page::Menu);
    feedback_.clear();
}

void MenuBase::switchPage(const Page page) {
    page_ = page;
    page_index_ = static_cast<int>(page);
}

bool MenuBase::copyResult() {
#ifdef _WIN32
    if (result_.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        feedback_ = "The result could not be copied to the clipboard.";
        return false;
    }

    if (!OpenClipboard(nullptr)) {
        feedback_ = "The result could not be copied to the clipboard.";
        return false;
    }

    const int characterCount = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, result_.data(),
        static_cast<int>(result_.size()), nullptr, 0);
    if (characterCount == 0 && !result_.empty()) {
        CloseClipboard();
        feedback_ = "The result could not be copied to the clipboard.";
        return false;
    }

    const SIZE_T byteCount = (static_cast<SIZE_T>(characterCount) + 1) * sizeof(wchar_t);
    HGLOBAL clipboardData = GlobalAlloc(GMEM_MOVEABLE, byteCount);
    if (!clipboardData) {
        CloseClipboard();
        feedback_ = "The result could not be copied to the clipboard.";
        return false;
    }

    auto* wideText = static_cast<wchar_t*>(GlobalLock(clipboardData));
    if (!wideText) {
        GlobalFree(clipboardData);
        CloseClipboard();
        feedback_ = "The result could not be copied to the clipboard.";
        return false;
    }

    if (characterCount > 0) {
        const int converted = MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, result_.data(),
            static_cast<int>(result_.size()), wideText, characterCount);
        if (converted != characterCount) {
            GlobalUnlock(clipboardData);
            GlobalFree(clipboardData);
            CloseClipboard();
            feedback_ = "The result could not be copied to the clipboard.";
            return false;
        }
    }
    wideText[characterCount] = L'\0';
    GlobalUnlock(clipboardData);

    if (!EmptyClipboard() || !SetClipboardData(CF_UNICODETEXT, clipboardData)) {
        GlobalFree(clipboardData);
        CloseClipboard();
        feedback_ = "The result could not be copied to the clipboard.";
        return false;
    }

    CloseClipboard();
    feedback_ = "Copied to the clipboard.";
    return true;
#else
    feedback_ = "Clipboard access is only available on Windows.";
    return false;
#endif
}

Component MenuBase::getComponent() {
    action_labels_.clear();
    for (const auto& action : actions_) {
        action_labels_.push_back(action.label);
    }

    MenuOption actionMenuOptions;
    actionMenuOptions.on_enter = [this] {
        if (selected_action_ >= 0 &&
            selected_action_ < static_cast<int>(actions_.size())) {
            actions_[selected_action_].activate();
        }
    };
    actionMenuOptions.entries_option.transform = [this](const EntryState& state) {
        const bool active = state.active && action_focused_;
        const std::string label = (active ? "> " : "  ") + state.label;
        auto item = text(label);
        if (active) {
            item = item | color(selection_color_) | bold;
        }
        return item;
    };
    auto actionMenu = Menu(&action_labels_, &selected_action_,
                           std::move(actionMenuOptions));

    MenuOption footerMenuOptions;
    footerMenuOptions.on_enter = [this] {
        if (footer_action_) {
            footer_action_();
        }
    };
    footerMenuOptions.entries_option.transform = [this](const EntryState& state) {
        const bool active = state.active && footer_focused_;
        const std::string label = (active ? "> " : "  ") + state.label;
        auto item = text(label);
        if (active) {
            item = item | color(selection_color_) | bold;
        }
        return item;
    };
    auto footerMenu = Menu(&footer_labels_, &selected_footer_,
                           std::move(footerMenuOptions));

    auto menuContainer = Container::Vertical({actionMenu, footerMenu});

    auto menuPage = Renderer(menuContainer, [this, actionMenu, footerMenu] {
        action_focused_ = actionMenu->Focused();
        footer_focused_ = footerMenu->Focused();
        Elements headerElements;
        if (header_lines_) {
            for (const auto& line : header_lines_()) {
                headerElements.push_back(text(line.text) | bold | color(line.color) | center);
            }
        } else {
            std::istringstream titleLines(title_);
            std::string titleLine;
            while (std::getline(titleLines, titleLine)) {
                headerElements.push_back(text(titleLine) | bold | color(Color::Cyan) | center);
            }
        }
        return vbox({
                   vbox(headerElements),
                   separator(),
                   actionMenu->Render(),
                   text(" "),
                   footerMenu->Render(),
                   separator(),
                   text("Up/Down: navigate | Enter: select | Esc: " + footer_label_)
                       | dim | center
               }) | border | size(WIDTH, EQUAL, width_) | center;
    });

    InputOption inputOptions;
    inputOptions.on_enter = [this] {
        if (submit_) {
            submit_();
        }
    };
    auto input = Input(&input_, "Enter a value...", std::move(inputOptions));
    auto submit = Button("Accept", [this] {
        if (submit_) {
            submit_();
        }
    });
    auto cancelPrompt = Button("Cancel", [this] {
        returnToMenu();
    });
    auto promptContainer = Container::Vertical({
        input,
        Container::Horizontal({submit, cancelPrompt})
    });
    auto promptPage = Renderer(promptContainer, [this, input, submit, cancelPrompt] {
        return vbox({
                   text(prompt_) | bold | color(Color::Cyan) | center,
                   separator(),
                   text("Input:") | bold,
                   input->Render() | border,
                   paragraph(feedback_) | color(Color::Red),
                   separator(),
                   hbox({submit->Render(), text("  "), cancelPrompt->Render()}) | center,
                   separator(),
                   text("Esc: cancel") | dim | center
               }) | border | size(WIDTH, EQUAL, width_) | center;
    });

    MenuOption choiceMenuOptions;
    choiceMenuOptions.on_enter = [this] {
        if (selected_choice_ >= 0 &&
            selected_choice_ < static_cast<int>(choice_actions_.size())) {
            choice_actions_[selected_choice_]();
        }
    };
    choiceMenuOptions.entries_option.transform = [this](const EntryState& state) {
        const std::string label = (state.active ? "> " : "  ") + state.label;
        auto item = text(label);
        if (state.active) {
            item = item | color(selection_color_) | bold;
        }
        return item;
    };
    auto choiceMenu = Menu(&choice_labels_, &selected_choice_,
                           std::move(choiceMenuOptions));
    auto cancelChoice = Button("Cancel", [this] {
        returnToMenu();
    });
    auto choiceContainer = Container::Vertical({choiceMenu, cancelChoice});
    auto choicePage = Renderer(choiceContainer, [this, choiceMenu, cancelChoice] {
        return vbox({
                   text(prompt_) | bold | color(Color::Cyan) | center,
                   separator(),
                   choiceMenu->Render(),
                   text(" "),
                   cancelChoice->Render(),
                   separator(),
                   text("Up/Down: navigate | Enter: select | Esc: Back") | dim | center
               }) | border | size(WIDTH, EQUAL, width_) | center;
    });

    auto copy = Button("Copy", [this] {
        copyResult();
    });
    auto backResult = Button("Back", [this] {
        returnToMenu();
    });
    auto resultContainer = Container::Horizontal({copy, backResult});
    auto resultPage = Renderer(resultContainer, [this, copy, backResult] {
        return vbox({
                   text("Result") | bold | color(Color::Cyan) | center,
                   separator(),
                   paragraph(result_) | border | flex,
                   paragraph(feedback_) | color(Color::GreenYellow),
                   separator(),
                   hbox({copy->Render(), text("  "), backResult->Render()}) | center,
                   separator(),
                   text("Esc: back") | dim | center
               }) | border | size(WIDTH, EQUAL, width_) | center;
    });

    auto pages = Container::Tab(
        {menuPage, promptPage, choicePage, resultPage},
        &page_index_);
    return CatchEvent(pages, [this](Event event) {
        if (event != Event::Escape) {
            return false;
        }
        if (page_ == Page::Menu) {
            if (footer_action_) {
                footer_action_();
            }
        } else {
            returnToMenu();
        }
        return true;
    });
}
