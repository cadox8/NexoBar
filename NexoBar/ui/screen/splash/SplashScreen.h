#pragma once

#include "../IScreen.h"
#include "../../../manager/AssetManager.h"

class SplashScreen final : public IScreen {
public:
    SplashScreen() = default;

    void update(float deltaSeconds) override;

    void draw() override;

private:
    float m_elapsed = 0;
    TexturePtr m_logo;
    bool m_assetsLoaded = false;
    bool m_transitioned = false;
};

using SplashScreenPtr = std::shared_ptr<SplashScreen>;
