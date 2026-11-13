#pragma once

#include "../../../manager/AssetManager.h"
#include "../button/Button.h"
#include "../core/IComponent.h"

#include <functional>
#include <string>
#include <vector>

// A settings row keeps a short context label and any number of equally-sized
// actions.
class SettingsActionRow final : public IComponent {
public:
    struct Action {
        std::string text;
        std::function<void()> callback;
        bool destructive = false;
    };

    SettingsActionRow(std::string label, FontPtr font,
                      std::vector<Action> actions);

    void update(float deltaSeconds) override;

    void draw() override;

    void setFontSize(int value);

    void setLabel(std::string value) { m_label = std::move(value); }

    ButtonPtr button(std::size_t index) const;

private:
    void layout();

    std::string m_label;
    FontPtr m_font;
    int m_fontSize = 17;
    std::vector<ButtonPtr> m_buttons;
};

using SettingsActionRowPtr = std::shared_ptr<SettingsActionRow>;
