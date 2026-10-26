#pragma once
#include "../ui/screen/IScreen.h"
#include "../ui/screen/ScreenLayout.h"

#include <string>
#include <unordered_map>
#include <vector>

class ScreenManager {
public:
    void registerScreen(std::string name, IScreenPtr screen);

    bool changeScreen(const std::string& name);

    void set(IScreenPtr screen);

    bool setLayer(std::string name, IScreenPtr screen);

    bool setLayers(std::vector<ScreenLayout::ScreenEntry> screens);

    bool removeLayer(const std::string& name);

    void push(IScreenPtr screen);

    void pop();

    [[nodiscard]] IScreenPtr current() const;

    void update(float deltaSeconds);

    void draw();

    void reset();

private:
    std::unordered_map<std::string, IScreenPtr> m_registeredScreens;
    std::string m_currentScreenName;
    std::vector<IScreenPtr> m_screens;
};

using ScreenManagerPtr = std::shared_ptr<ScreenManager>;
