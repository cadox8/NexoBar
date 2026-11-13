#pragma once

#include "../../components/button/Button.h"
#include "../../components/layout/ScrollableBlock.h"
#include "../../components/text/StaticText.h"
#include "../IScreen.h"
#include "database/models/Ticket.h"
#include "database/models/CashSession.h"
#include "database/hospitality/Types.h"
#include "../../components/layout/BlockComponent.h"
#include "../../../fiscal/FiscalService.h"

#include <boost/date_time/gregorian/gregorian.hpp>
#include <vector>

class ReportScreen final : public IScreen {
public:
    enum class Period { Current, Day, Month, Year };
    ReportScreen(nexobar::UserId userId, bool canPrint = true, bool canClose = false, bool canExport = false);
    void activate();
    void update(float deltaSeconds) override;
    void draw() override;

private:
    void setPeriod(Period period);
    void move(int amount);
    void refresh();
    void print();
    void closeAndPrint();
    void exportFiscal(nexobar::FiscalExportFormat format);
    void exportDeclaration();
    void verifyFiscal();
    void layout();
    [[nodiscard]] std::string periodLabel() const;
    [[nodiscard]] bool includes(const nexobar::Ticket& ticket) const;
    [[nodiscard]] bool includes(nexobar::Timestamp timestamp) const;
    [[nodiscard]] std::string reportText() const;
    void updatePeriodButtonStyles();

    ScrollableBlockPtr m_root;
    StaticTextPtr m_title, m_periodLabel, m_summary, m_status;
    BlockComponentPtr m_periodActions, m_dateActions, m_printActions, m_fiscalActions, m_summaryCard;
    ButtonPtr m_current, m_day, m_month, m_year, m_previous, m_next, m_print, m_close, m_exportXml, m_exportJson, m_declaration, m_verifyFiscal;
    std::vector<nexobar::Ticket> m_tickets;
    std::vector<nexobar::CashSession> m_cashSessions;
    Period m_period = Period::Current;
    boost::gregorian::date m_date;
    nexobar::UserId m_userId{};
};
