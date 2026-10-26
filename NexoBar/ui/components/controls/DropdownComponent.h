#pragma once
#include "../core/IComponent.h"
#include "../../../manager/AssetManager.h"
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include <utility>

struct DropdownOption {
    std::string label, value;
};

class DropdownComponent final : public IComponent {
public:
    std::vector<DropdownOption> options;
    int selectedIndex = -1;
    bool open = false;
    FontPtr font;
    int fontSize = 20, maxVisibleOptions = 7;
    float optionHeight = 38;
    Color background{42, 50, 64, 255}, popupBackground{35, 42, 55, 255}, hoverBackground{63, 83, 110, 255}, textColor = RAYWHITE, borderColor{90, 102, 120, 255};
    std::function<void(int, const DropdownOption&)> onSelectionChanged;

    void setOptions(std::vector<DropdownOption> values, bool preserveSelection = true);

    const DropdownOption* selected() const noexcept;

    void select(int index);

    void update(float deltaSeconds) override;

    void draw() override;

    void drawOverlay();

private:
    int m_firstVisible = 0, m_hovered = -1;

    Rectangle popupArea() const;
};

using DropdownComponentPtr = std::shared_ptr<DropdownComponent>;
