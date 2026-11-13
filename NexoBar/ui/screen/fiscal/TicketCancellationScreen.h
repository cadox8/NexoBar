#pragma once

#include "../../components/button/Button.h"
#include "../../components/controls/DropdownComponent.h"
#include "../../components/layout/ScrollableBlock.h"
#include "../../components/text/InputText.h"
#include "../../components/text/StaticText.h"
#include "../IScreen.h"
#include "database/hospitality/Types.h"

class TicketCancellationScreen final : public IScreen {
public:
    explicit TicketCancellationScreen(nexobar::UserId userId);
    void update(float deltaSeconds) override;
    void draw() override;

private:
    void refresh();
    void cancelSelected();
    void layout();

    ScrollableBlockPtr m_root;
    StaticTextPtr m_title, m_help, m_status;
    DropdownComponentPtr m_tickets;
    InputTextPtr m_reason;
    ButtonPtr m_cancel;
    FontPtr m_font;
    nexobar::UserId m_userId{};
};
