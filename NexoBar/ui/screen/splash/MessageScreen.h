#pragma once

#include "../IScreen.h"
#include "../../../manager/AssetManager.h"

#include <string>
#include <vector>

class MessageScreen final : public IScreen {
public:
    MessageScreen(std::string title, std::vector<std::string> errors, std::vector<std::string> warnings,
                  std::string warningPrefix, FontPtr font = {});

    void update(float deltaSeconds) override;

    void draw() override;

private:
    std::string m_title;
    std::vector<std::string> m_errors;
    std::vector<std::string> m_warnings;
    std::string m_warningPrefix;
    FontPtr m_font;
};
