#pragma once

#include "../../components/button/Button.h"
#include "../../components/layout/ScrollableBlock.h"
#include "../../components/text/StaticText.h"
#include "../IScreen.h"
#include "database/models/Ticket.h"
#include "database/hospitality/Types.h"
#include "../../components/layout/BlockComponent.h"

#include <boost/date_time/gregorian/gregorian.hpp>
#include <vector>

class ReportScreen final : public IScreen {
public:
    enum class Period { Day, Month, Year };
    ReportScreen(nexobar::UserId userId, bool canPrint = true, bool canClose = false);
    void update(float deltaSeconds) override;
    void draw() override;

private:
    void setPeriod(Period period);
    void move(int amount);
    void refresh();
    void print();
    void closeAndPrint();
    void layout();
    [[nodiscard]] std::string periodLabel() const;
    [[nodiscard]] bool includes(const nexobar::Ticket& ticket) const;
    [[nodiscard]] std::string reportText() const;

    ScrollableBlockPtr m_root;
    StaticTextPtr m_title, m_periodLabel, m_summary, m_status;
    BlockComponentPtr m_periodActions, m_dateActions, m_printActions;
    ButtonPtr m_day, m_month, m_year, m_previous, m_next, m_print, m_close;
    std::vector<nexobar::Ticket> m_tickets;
    Period m_period = Period::Day;
    boost::gregorian::date m_date;
    nexobar::UserId m_userId{};
};
