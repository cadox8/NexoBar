#include "LoadingScreen.h"

#include "../../../NexoBar.h"

#include <algorithm>

LoadingScreen::LoadingScreen()
    : m_title(std::make_shared<StaticText>()), m_status(std::make_shared<StaticText>()),
      m_messages(std::make_shared<StaticText>()), m_progress(std::make_shared<ProgressBar>()) {
    NexoBarImp nexo;
    m_font = nexo->defaultFont();
    for (const auto& text : {m_title, m_status, m_messages}) {
        text->font = m_font;
        text->color = RAYWHITE;
        text->wrap = true;
    }
    m_title->text = nexo->text("loading.title");
    m_status->color = Color{190, 199, 214, 255};
}
void LoadingScreen::layout() {
    const float width = std::clamp(m_area.width * .72f, 280.f, 900.f);
    const float x = m_area.x + (m_area.width - width) * .5f;
    const float y = m_area.y + std::max(28.f, m_area.height * .18f);
    m_title->area({x, y, width, 54.f});
    m_progress->area({x, y + 72.f, width, 18.f});
    m_status->area({x, y + 104.f, width, 46.f});
    m_messages->area({x, y + 160.f, width, std::max(80.f, m_area.height - y - 190.f)});
    m_title->fontSize = std::clamp(std::min(m_area.width, m_area.height) * .06f, 28.f, 44.f);
    m_status->fontSize = m_messages->fontSize = 18.f;
}
void LoadingScreen::update(const float deltaSeconds) {
    NexoBarImp nexo;
    if (!nexo->assets().ready()) nexo->assets().preloadNext();
    const auto total = nexo->assets().totalCount();
    m_progress->value = total ? static_cast<float>(nexo->assets().completedCount()) / static_cast<float>(total) : 1.f;
    m_status->text = nexo->text(nexo->assets().ready() ? "loading.complete" : "loading.resource");
    replaceToken(m_status->text, "{resource}", nexo->assets().loadingResource());
    std::string messages;
    for (const auto& warning : nexo->startupWarnings())
        messages += nexo->text("warnings.warning_prefix") + warning + '\n';
    for (const auto& error : nexo->startupErrors())
        messages += error + '\n';
    m_messages->text = messages;
    m_messages->color = nexo->startupErrors().empty() ? Color{255, 205, 125, 255} : Color{255, 145, 135, 255};
    layout();
    m_title->update(deltaSeconds); m_progress->update(deltaSeconds);
    m_status->update(deltaSeconds); m_messages->update(deltaSeconds);
    if (nexo->assets().ready()) {
        if (nexo->canEnterSession())
            nexo->showLogin();
        else
            nexo->screens().changeScreen("errors");
    }
}
void LoadingScreen::draw() {
    layout();
    m_title->draw(); m_progress->draw(); m_status->draw(); m_messages->draw();
}
