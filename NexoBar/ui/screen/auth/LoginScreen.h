#pragma once

#include "../../components/button/Button.h"
#include "../../components/layout/BlockComponent.h"
#include "../../components/text/StaticText.h"
#include "../IScreen.h"

#include <cstdint>
#include <functional>
#include <vector>
#include <optional>
#include "database/models/User.h"

class LoginScreen final : public IScreen {
public:
    explicit LoginScreen(std::function<void(std::int64_t)> login);
    void update(float deltaSeconds) override;
    void draw() override;
private:
    void layout();
    void selectUser(const nexobar::User& user);
    void pinKey(const std::string& key);
    std::function<void(std::int64_t)> m_login;
    BlockComponentPtr m_root, m_pinRoot;
    StaticTextPtr m_title, m_subtitle, m_pinTitle, m_pinDisplay, m_pinStatus;
    std::vector<ButtonPtr> m_users;
    std::vector<ButtonPtr> m_pinButtons;
    ButtonPtr m_cancel;
    std::optional<nexobar::User> m_pendingUser;
    std::string m_pin;
};
