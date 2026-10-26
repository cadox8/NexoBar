#include "HeaderScreen.h"

#include "../../../NexoBar.h"

#include <raylib.h>

#include <algorithm>
#include <stdexcept>
#include <string>

HeaderScreen::HeaderScreen(std::shared_ptr<ProductState> state)
    : m_state(std::move(state)),
      m_background(std::make_shared<BoxComponent>(Panel)),
      m_context(std::make_shared<StaticText>()),
      m_clock(std::make_shared<StaticText>()) {

    NexoBar* app = NexoBarImp::get();

    m_font = app->defaultFont();
    m_context->font = m_font;
    m_context->fontSize = 20;
    m_context->color = RAYWHITE;
    m_context->verticalAlignment = TextVerticalAlignment::Center;
    m_context->horizontalAlignment = TextHorizontalAlignment::Left;
    m_clock->font = m_font;
    m_clock->fontSize = 20;
    m_clock->color = RAYWHITE;
    m_clock->verticalAlignment = TextVerticalAlignment::Center;
    m_clock->horizontalAlignment = TextHorizontalAlignment::Right;
}

void HeaderScreen::layout() {
    const auto width = m_area.width;
    const auto height = m_area.height;
    const float unit = std::max(1.f, std::min(width, height));
    const float inset = std::min(height * .14f, 12.f);
    const float clockWidth = width * .28f;
    m_background->area(m_area);
    m_context->text = NexoBarImp::get()->text("header.context_format", "{business} - {user}");
    replaceToken(m_context->text, "{business}", m_state->businessName);
    replaceToken(m_context->text, "{user}", m_state->userName);
    m_context->fontSize = m_clock->fontSize = std::clamp(unit * .40f, 12.f, 18.f);
    m_context->area({.x = m_area.x + inset, .y = m_area.y, .width = std::max(0.f, width - clockWidth - inset * 2), .height = height});
    m_clock->area({
        .x = m_area.x + width - clockWidth - inset, .y = m_area.y,
        .width = std::max(0.f, clockWidth), .height = height
    });
}

void HeaderScreen::updateClock() {
    NexoBarImp app;

    std::time_t now = std::time(nullptr);
    std::tm localTime{};
#ifdef _WIN32
    localtime_s(&localTime, &now);
#else
    localtime_r(&now, &localTime);
#endif
    const std::string timeFormat = app->text("header.clock_format", "%H:%M:%S");
    char formattedTime[64]{};
    if (std::strftime(formattedTime, sizeof(formattedTime), timeFormat.c_str(), &localTime) == 0) {
        const std::string message = app->text("errors.clock_format_failed", "Could not update the clock.");
        throw std::runtime_error(message);
    }
    m_clock->text = app->text("header.time_format", "Time: {time}");
    replaceToken(m_clock->text, "{time}", formattedTime);
}

void HeaderScreen::update(const float) {
    layout();
    updateClock();
}

void HeaderScreen::draw() {
    layout();
    m_background->draw();
    m_context->draw();
    m_clock->draw();
}
