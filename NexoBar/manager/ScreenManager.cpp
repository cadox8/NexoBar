#include "ScreenManager.h"

#include "../ui/components/core/IComponent.h"

#include "../ui/screen/IScreen.h"
#include "../ui/screen/ScreenLayout.h"

void ScreenManager::registerScreen(std::string name, IScreenPtr screen) {
    if (!name.empty() && screen)
        m_registeredScreens.insert_or_assign(std::move(name), std::move(screen));
}

bool ScreenManager::changeScreen(const std::string& name) {
    const auto it = m_registeredScreens.find(name);
    if (it == m_registeredScreens.end())
        return false;
    m_currentScreenName = name;
    m_screens.assign(1, it->second);
    return true;
}

void ScreenManager::set(IScreenPtr screen) {
    m_currentScreenName.clear();
    m_screens.clear();
    if (screen)
        m_screens.push_back(std::move(screen));
}

bool ScreenManager::setLayer(std::string name, IScreenPtr screen) {
    const auto layout = std::dynamic_pointer_cast<ScreenLayout>(current());
    if (!layout)
        return false;
    layout->setScreen(std::move(name), std::move(screen));
    return true;
}

bool ScreenManager::setLayers(std::vector<ScreenLayout::ScreenEntry> screens) {
    const auto layout = std::dynamic_pointer_cast<ScreenLayout>(current());
    if (!layout)
        return false;
    layout->setScreens(std::move(screens));
    return true;
}

bool ScreenManager::removeLayer(const std::string& name) {
    const auto layout = std::dynamic_pointer_cast<ScreenLayout>(current());
    return layout && layout->removeScreen(name);
}

void ScreenManager::push(IScreenPtr screen) {
    if (screen)
        m_screens.push_back(std::move(screen));
}

void ScreenManager::pop() {
    if (!m_screens.empty())
        m_screens.pop_back();
}

IScreenPtr ScreenManager::current() const { return m_screens.empty() ? nullptr : m_screens.back(); }

void ScreenManager::update(float dt) {
    UIInput::beginFrame();
    if (const auto active = current()) {
        active->area({0.f, 0.f, static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight())});
        active->update(dt);
        UIInput::endFrame();
    }
}

void ScreenManager::draw() {
    for (const auto& active: m_screens) {
        active->area({0.f, 0.f, static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight())});
        active->draw();
    }
}

void ScreenManager::reset() {
    m_screens.clear();
    m_registeredScreens.clear();
    m_currentScreenName.clear();
}
