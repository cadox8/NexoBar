# Management menus: developer guide

This guide describes how to extend and customize the Management menu code. It
covers the menu classes and their lifecycle, action types, input parsing,
results, navigation, selection styling, and clipboard behavior.

## Architecture

- `menus/MenuBase.h` and `menus/MenuBase.cpp` provide the reusable menu
  framework. The base class owns action definitions, page state, input/result
  screens, selection styling, and clipboard support.
- Each concrete menu derives from `MenuBase`. Its header declares the class and
  its `.cpp` implements `init()`. Define menu items in `init()` rather than
  putting them in the class header.
- `Management::init()` initializes concrete menus, connects their callbacks,
  creates one FTXUI `Container::Tab`, and runs one `ScreenInteractive` loop.
  Menu callbacks switch the active tab; they do not start nested screen loops.
- `Management/CMakeLists.txt` must list each new menu implementation file in
  `MENUS`.

The base menu has four internal pages: `Menu`, `Prompt`, `Choice`, and
`Result`. Actions and page navigation switch this state; the tab container
shows the matching FTXUI component.

## Defining a menu

Declare a class with no menu-item data in its header:

```cpp
#pragma once

#include "MenuBase.h"

class ExampleMenu final : public MenuBase {
public:
    void init() override;
};
```

Build its title, width, items, and footer in the implementation:

```cpp
#include "ExampleMenu.h"

void ExampleMenu::init() {
    beginMenu("Example tools", "Back", 70);
    addAction("Run example", [] {
        // Perform an action that does not need a prompt.
    });
    setFooterAction("Back", [this] {
        if (on_back) {
            on_back();
        }
    });
}
```

`beginMenu(title, footerLabel, width)` starts/reinitializes a menu: it clears
its actions and result state, resets selection, and returns to the menu page.
Call it first in `init()`. The footer is rendered as a plain menu row, separated
from regular actions by a blank line. It is invoked by selecting that row or
pressing Escape while on the menu page.

Use `addAction(label, callback)` for an immediate action. It is protected so a
derived menu can add custom behavior without relying on a button index or ID.
The callback attached to the selected button determines what happens.

`setFooterAction(label, callback)` defines the separated footer action.
`on_back`, `on_exit`, and `on_open_licenses` are navigation callbacks wired by
the owner in `Management::init()`. A menu should call the appropriate callback
instead of creating another screen loop.

## Prompt actions and typed input

`addPromptAction<T>(label, prompt, parse, createResult)` creates an action that
opens an input page. Pressing Enter in the field or activating **Submit** calls
the parser. The parser returns `std::optional<T>`:

- A value means the input was accepted and is passed to `createResult`.
- `std::nullopt` keeps the prompt open and displays the validation error.
- `createResult` returns the exact string displayed on the result page.

For free-form text, use `MenuBase::parseText`. For integers, use
`MenuBase::parseInteger<T>`; it requires the entire input to be a valid integer
and rejects overflow or trailing characters.

```cpp
addPromptAction<int>(
    "Set quantity",
    "Enter a quantity",
    MenuBase::parseInteger<int>,
    [](const int& quantity) {
        return "Quantity accepted: " + std::to_string(quantity);
    });
```

For paths, validated text, or another input type, provide a parser that
validates and converts the string:

```cpp
addPromptAction<std::string>(
    "Choose output path",
    "Enter an output path",
    [](const std::string& input) -> std::optional<std::string> {
        if (input.empty()) {
            return std::nullopt;
        }
        return input;
    },
    [](const std::string& path) {
        return "Selected path:\n" + path;
    });
```

The prompt UI and its Submit/Cancel buttons are shared by every prompt action.
Cancel and Escape return to the action list. Validation failures do not replace
the result page or discard the prompt input.

## Choice actions

`addChoiceAction<T>(label, prompt, choices, createResult)` creates an action
whose next page is a typed choice list. Each item is a display-label/value
pair. Selecting an item passes its value to the result callback:

```cpp
enum class ExportFormat {
    Csv,
    Json
};

addChoiceAction<ExportFormat>(
    "Export data",
    "Select an export format",
    {
        {"CSV", ExportFormat::Csv},
        {"JSON", ExportFormat::Json}
    },
    [](const ExportFormat& format) {
        return format == ExportFormat::Csv
                   ? "CSV export is ready."
                   : "JSON export is ready.";
    });
```

Choice pages add a separated **Cancel** entry automatically. Escape and Cancel
return to the action list; selecting a choice opens the result page.

## Results and clipboard

Every action result replaces the previous result. `createResult` should return
only the content that should be displayed or copied. The result page displays
that exact string and provides **Copy to clipboard** and **Back** actions.
This is useful for generated keys, paths, commands, or any other text. For
example, the license generator returns only the generated license code, not a
label or explanatory sentence.

Clipboard copying currently uses the Windows Unicode clipboard API. Errors,
such as an unavailable clipboard or invalid UTF-8, are shown on the result
page. On other platforms, the action reports that clipboard copying is not
supported; add a platform-specific implementation to `MenuBase.cpp` if the
application is ported.

## Header and colors

`setHeaderLines(callback)` replaces the default title header with a list of
`HeaderLine{text, color}` values. Keep these lines stable while navigating;
they identify the current menu (for example, application name, submenu name,
and version) and are independent of the highlighted action.

`setSelectionColor(color)` changes the active action and choice color. The
active item is marked with `>` and rendered bold in this color. Footer actions
are plain menu rows, separated from regular actions by a blank line. A menu
can customize its header and version independently:

```cpp
setSelectionColor(ftxui::Color::BlueLight);
setHeaderLines([] {
    return std::vector<HeaderLine>{
        {"NexoBar Management", ftxui::Color::Cyan},
        {"License management", ftxui::Color::BlueLight},
        {"Version: 1.0.0", ftxui::Color::LightCoral}
    };
});
```

`MainMenu::setVersionColor(ftxui::Color)` changes only the version line. Call it
before or after `init()`; the header reads the current color on each render.
The default is `ftxui::Color::LightCoral`.

## Adding a menu to Management

1. Add the menu `.h` and `.cpp`; implement `init()` using the base helpers.
2. Add the `.cpp` file to the `MENUS` list in `Management/CMakeLists.txt`.
3. Construct the menu in `Management::init()` and call its `init()`.
4. Connect navigation callbacks to the owner-managed active tab.
5. Add its component to the existing `Container::Tab`. Do not call
   `ScreenInteractive::Loop()` from an individual menu.

For a new interactive page type beyond the existing menu, prompt, choice, and
result cases, extend `MenuBase::Page` and build its FTXUI component in
`MenuBase::getComponent()`. Keep page-specific state in the base or in a typed
helper, and route transitions through `switchPage()` so the visible tab and
internal page state remain synchronized.
