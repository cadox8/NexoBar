#include "LoginScreen.h"

#include "../../../NexoBar.h"
#include "database/models/User.h"
#include "security/Pin.h"

#include <algorithm>

LoginScreen::LoginScreen(std::function<void(std::int64_t)> login)
    : m_login(std::move(login)),
      m_root(std::make_shared<BlockComponent>(LayoutAxis::Vertical, Color{18, 23, 32, 255})),
      m_pinRoot(std::make_shared<BlockComponent>(LayoutAxis::Vertical, Color{18, 23, 32, 255})),
      m_title(std::make_shared<StaticText>()), m_subtitle(std::make_shared<StaticText>()),
      m_pinTitle(std::make_shared<StaticText>()), m_pinDisplay(std::make_shared<StaticText>()),
      m_pinStatus(std::make_shared<StaticText>()) {
    NexoBarImp nexo;
    m_font = nexo->defaultFont();
    for (const auto& text : {m_title, m_subtitle, m_pinTitle, m_pinDisplay, m_pinStatus}) text->font = m_font;
    m_title->color = RAYWHITE;
    m_subtitle->color = Color{170, 184, 205, 255};
    m_pinTitle->color = m_pinDisplay->color = RAYWHITE;
    m_pinStatus->color = Color{255, 135, 125, 255};
    m_pinTitle->horizontalAlignment = m_pinDisplay->horizontalAlignment = TextHorizontalAlignment::Center;
    m_title->text = nexo->text("auth.title");
    m_subtitle->text = nexo->text("auth.select_user");
    m_root->content.add(m_title, LayoutSize::fill(), LayoutSize::px(70));
    m_root->content.add(m_subtitle, LayoutSize::fill(), LayoutSize::px(44));
    if (nexo->database()) {
        for (const auto& user : nexo->database()->query<nexobar::User>()
                 .where(Nearon::col<&nexobar::User::active> == true)
                 .orderBy(Nearon::col<&nexobar::User::displayName>.asc()).all()) {
            auto button = std::make_shared<Button>(user.displayName, [this, user] { selectUser(user); });
            button->font = m_font;
            button->fontSize = 20;
            m_users.push_back(button);
            m_root->content.add(button, LayoutSize::fill(), LayoutSize::px(58));
        }
    }
    m_pinRoot->content.add(m_pinTitle, LayoutSize::fill(), LayoutSize::px(70));
    m_pinRoot->content.add(m_pinDisplay, LayoutSize::fill(), LayoutSize::px(58));
    const std::string labels[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "DEL", "0", "OK"};
    for (int rowIndex = 0; rowIndex < 4; ++rowIndex) {
        auto row = std::make_shared<BlockComponent>(LayoutAxis::Horizontal);
        row->content.gap(10.f);
        for (int column = 0; column < 3; ++column) {
            const std::string key = labels[rowIndex * 3 + column];
            const std::string label = key == "DEL" ? nexo->text("auth.delete_key")
                                      : key == "OK" ? nexo->text("auth.confirm_key") : key;
            auto button = std::make_shared<Button>(label, [this, key] { pinKey(key); });
            button->font = m_font;
            button->fontSize = 22;
            m_pinButtons.push_back(button);
            row->content.add(button, LayoutSize::fill(), LayoutSize::fill());
        }
        m_pinRoot->content.add(row, LayoutSize::fill(), LayoutSize::px(58));
    }
    m_pinRoot->content.add(m_pinStatus, LayoutSize::fill(), LayoutSize::px(42));
    m_cancel = std::make_shared<Button>(nexo->text("auth.cancel"), [this] { m_pendingUser.reset(); m_pin.clear(); });
    m_cancel->font = m_font;
    m_pinRoot->content.add(m_cancel, LayoutSize::fill(), LayoutSize::px(52));
}
void LoginScreen::selectUser(const nexobar::User& user) {
    if (user.pinHash.empty()) { m_login(user.id.get()); return; }
    NexoBarImp app;
    m_pendingUser = user;
    m_pin.clear();
    m_pinStatus->text.clear();
    m_pinTitle->text = app->text("auth.pin_for") + user.displayName;
    m_pinDisplay->text = "";
}
void LoginScreen::pinKey(const std::string& key) {
    if (!m_pendingUser) return;
    if (key == "DEL") { if (!m_pin.empty()) m_pin.pop_back(); }
    else if (key == "OK") {
        NexoBarImp app;
        if (nexobar::Pin::valid(m_pin) && nexobar::Pin::matches(m_pin, m_pendingUser->pinSalt, m_pendingUser->pinHash)) {
            const auto id = m_pendingUser->id.get();
            m_pin.clear(); m_pendingUser.reset(); m_login(id); return;
        }
        m_pin.clear();
        m_pinStatus->text = app->text("auth.pin_invalid");
    } else if (m_pin.size() < 6) m_pin += key;
    m_pinDisplay->text.assign(m_pin.size(), '*');
}
void LoginScreen::layout() {
    const float unit = std::max(1.f, std::min(m_area.width, m_area.height));
    const float horizontal = std::max(20.f, (m_area.width - std::clamp(m_area.width * .55f, 320.f, 680.f)) * .5f);
    m_root->content.padding({horizontal, std::clamp(unit * .12f, 32.f, 100.f), horizontal, 24.f}).gap(12.f);
    m_root->area(m_area);
    m_title->fontSize = std::clamp(unit * .075f, 30.f, 48.f);
    m_subtitle->fontSize = std::clamp(unit * .034f, 17.f, 23.f);
    m_pinRoot->content.padding({horizontal, std::clamp(unit * .07f, 24.f, 62.f), horizontal, 20.f}).gap(10.f);
    m_pinRoot->area(m_area);
    m_pinTitle->fontSize = m_title->fontSize;
    m_pinDisplay->fontSize = std::clamp(unit * .055f, 25.f, 38.f);
    m_pinStatus->fontSize = m_subtitle->fontSize;
}
void LoginScreen::update(const float deltaSeconds) { layout(); (m_pendingUser ? m_pinRoot : m_root)->update(deltaSeconds); }
void LoginScreen::draw() { layout(); (m_pendingUser ? m_pinRoot : m_root)->draw(); }
