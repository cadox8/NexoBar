#pragma once

#include "../sales/ProductState.h"
#include "../../components/box/BoxComponent.h"
#include "../../components/text/StaticText.h"
#include "../IScreen.h"

#include <memory>

class HeaderScreen final : public IScreen {
public:
    explicit HeaderScreen(std::shared_ptr<ProductState> state);

    void update(float deltaSeconds) override;

    void draw() override;

private:
    void layout();

    void updateClock();

private:
    std::shared_ptr<ProductState> m_state;
    BoxComponentPtr m_background;
    StaticTextPtr m_context;
    StaticTextPtr m_clock;
    FontPtr m_font;
    float m_identityRefreshElapsed = 0.f;
};
