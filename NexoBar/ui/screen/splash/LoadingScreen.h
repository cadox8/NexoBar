#pragma once

#include "../../components/feedback/ProgressBar.h"
#include "../../components/text/StaticText.h"
#include "../IScreen.h"

class LoadingScreen final : public IScreen {
public:
    LoadingScreen();
    void update(float deltaSeconds) override;
    void draw() override;
private:
    void layout();
    StaticTextPtr m_title, m_status, m_messages;
    ProgressBarPtr m_progress;
};
