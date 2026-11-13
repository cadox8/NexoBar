#include "ReportScreen.h"

#include "../../../NexoBar.h"
#include "../../../fiscal/FiscalService.h"
#include "../../../printer/TicketPrinter.h"
#include "../../../reporting/CashDay.h"
#include "../../../reporting/DailyCloseReceipt.h"
#include "database/hospitality/Money.h"
#include "database/models/Payment.h"

#include <algorithm>
#include <sstream>

namespace {
    constexpr Color Panel{38, 52, 70, 255};
    constexpr Color PanelHover{49, 72, 96, 255};
    constexpr Color PanelPressed{31, 44, 60, 255};
    constexpr Color PanelBorder{70, 91, 118, 255};
    constexpr Color Selected{44, 116, 111, 255};
    constexpr Color SelectedBorder{84, 181, 162, 255};

    void settingsStyle(const ButtonPtr& button) {
        button->fontSize = 17;
        button->roundness = .18f;
        button->padding = LayoutInsets::symmetric(10.f, 6.f);
        button->background = Panel;
        button->hoverBackground = PanelHover;
        button->pressedBackground = PanelPressed;
        button->borderColor = PanelBorder;
        button->borderWidth = 1.f;
    }
}

ReportScreen::ReportScreen(const nexobar::UserId userId, const bool canPrint,
                           const bool canClose, const bool canExport)
    : m_root(std::make_shared<ScrollableBlock>(LayoutAxis::Vertical, Color{24, 31, 42, 255})),
      m_title(std::make_shared<StaticText>()), m_periodLabel(std::make_shared<StaticText>()),
      m_summary(std::make_shared<StaticText>()), m_status(std::make_shared<StaticText>()),
      m_date(boost::gregorian::day_clock::local_day()), m_userId(userId) {
    NexoBarImp app;
    m_font = app->defaultFont();
    for (const auto& text : {m_title, m_periodLabel, m_status}) {
        text->font = m_font;
        text->color = RAYWHITE;
        text->wrap = true;
    }
    m_title->text = app->text("reports.title");
    m_status->color = Color{126, 225, 185, 255};

    const auto make = [&](const std::string& key, auto action) {
        auto button = std::make_shared<Button>(app->text(key), action);
        button->font = m_font;
        settingsStyle(button);
        return button;
    };
    m_current = make("reports.current", [this] { setPeriod(Period::Current); });
    m_day = make("reports.day", [this] { setPeriod(Period::Day); });
    m_month = make("reports.month", [this] { setPeriod(Period::Month); });
    m_year = make("reports.year", [this] { setPeriod(Period::Year); });
    m_previous = make("reports.previous", [this] { move(-1); });
    m_next = make("reports.next", [this] { move(1); });
    m_print = make("reports.print", [this] { print(); });
    m_close = make("reports.close_and_print", [this] { closeAndPrint(); });
    m_exportXml = make("reports.export_xml", [this] { exportFiscal(nexobar::FiscalExportFormat::Xml); });
    m_exportJson = make("reports.export_json", [this] { exportFiscal(nexobar::FiscalExportFormat::Json); });
    m_declaration = make("reports.export_declaration", [this] { exportDeclaration(); });
    m_verifyFiscal = make("reports.verify_fiscal", [this] { verifyFiscal(); });
    m_print->enabled = canPrint;
    m_close->enabled = canClose;
    m_exportXml->enabled = m_exportJson->enabled = canExport;
    m_declaration->enabled = m_verifyFiscal->enabled = canExport;

    const auto row = [](const std::initializer_list<ButtonPtr> buttons) {
        auto result = std::make_shared<BlockComponent>(LayoutAxis::Horizontal);
        result->content.gap(10.f);
        for (const auto& button : buttons)
            result->content.add(button, LayoutSize::fill(), LayoutSize::fill());
        return result;
    };
    m_periodActions = row({m_current, m_day, m_month, m_year});
    m_dateActions = row({m_previous, m_next});
    m_printActions = row({m_print, m_close});
    m_fiscalActions = row({m_exportXml, m_exportJson, m_verifyFiscal, m_declaration});

    m_summary->font = m_font;
    m_summary->color = Color{35, 35, 33, 255};
    m_summary->wrap = false;
    m_summary->verticalAlignment = TextVerticalAlignment::Top;
    m_summaryCard = std::make_shared<BlockComponent>(LayoutAxis::Vertical, Color{248, 245, 235, 255});
    m_summaryCard->roundness = .025f;
    m_summaryCard->borderWidth = 1.f;
    m_summaryCard->borderColor = Color{190, 184, 170, 255};
    m_summaryCard->content.padding(LayoutInsets::all(20.f));
    m_summaryCard->content.add(m_summary, LayoutSize::fill(), LayoutSize::fill());

    m_root->content.add(m_title, LayoutSize::fill(), LayoutSize::px(58));
    m_root->content.add(m_periodActions, LayoutSize::fill(), LayoutSize::px(52));
    m_root->content.add(m_dateActions, LayoutSize::fill(), LayoutSize::px(50));
    m_root->content.add(m_periodLabel, LayoutSize::fill(), LayoutSize::px(42));
    m_root->content.add(m_printActions, LayoutSize::fill(), LayoutSize::px(52));
    m_root->content.add(m_fiscalActions, LayoutSize::fill(), LayoutSize::px(52));
    m_root->content.add(m_summaryCard, LayoutSize::fill(), LayoutSize::px(350));
    m_root->content.add(m_status, LayoutSize::fill(), LayoutSize::px(52));
    m_previous->enabled = m_next->enabled = false;
    m_close->visible = true;
    updatePeriodButtonStyles();
    refresh();
}

void ReportScreen::activate() {
    if (m_period == Period::Current) m_date = boost::gregorian::day_clock::local_day();
    refresh();
}

void ReportScreen::setPeriod(const Period period) {
    m_period = period;
    if (period == Period::Current) m_date = boost::gregorian::day_clock::local_day();
    m_previous->enabled = m_next->enabled = period != Period::Current;
    m_close->visible = period == Period::Current || period == Period::Day;
    updatePeriodButtonStyles();
    refresh();
}

void ReportScreen::updatePeriodButtonStyles() {
    const std::pair<ButtonPtr, Period> buttons[] = {
        {m_current, Period::Current}, {m_day, Period::Day},
        {m_month, Period::Month}, {m_year, Period::Year}
    };
    for (const auto& [button, period] : buttons) {
        button->background = period == m_period ? Selected : Panel;
        button->borderColor = period == m_period ? SelectedBorder : PanelBorder;
    }
}

void ReportScreen::move(const int amount) {
    if (m_period == Period::Current) return;
    if (m_period == Period::Day) m_date += boost::gregorian::days(amount);
    else if (m_period == Period::Month) m_date += boost::gregorian::months(amount);
    else m_date += boost::gregorian::years(amount);
    refresh();
}

bool ReportScreen::includes(const nexobar::Ticket& ticket) const {
    const auto date = nexobar::localTicketTime(ticket).date();
    if (m_period == Period::Current)
        return date == boost::gregorian::day_clock::local_day() &&
               !nexobar::isTicketCashDayClosed(m_cashSessions, ticket);
    if (m_period == Period::Day) return date == m_date;
    if (m_period == Period::Month) return date.year() == m_date.year() && date.month() == m_date.month();
    return date.year() == m_date.year();
}

bool ReportScreen::includes(const nexobar::Timestamp timestamp) const {
    const auto local = AppTime::toLocal(timestamp).local_time();
    const auto date = local.date();
    if (m_period == Period::Current) {
        if (date != boost::gregorian::day_clock::local_day()) return false;
        std::optional<nexobar::Timestamp> latest;
        for (const auto& session : m_cashSessions)
            if (session.openedAt.date() == date && session.closedAt &&
                (!latest || *session.closedAt > *latest))
                latest = session.closedAt;
        return !latest || local > *latest;
    }
    if (m_period == Period::Day) return date == m_date;
    if (m_period == Period::Month) return date.year() == m_date.year() && date.month() == m_date.month();
    return date.year() == m_date.year();
}

std::string ReportScreen::periodLabel() const {
    NexoBarImp app;
    std::ostringstream output;
    if (m_period == Period::Current) {
        output << app->text("reports.current_period") << " · "
               << boost::gregorian::to_iso_extended_string(boost::gregorian::day_clock::local_day());
    } else if (m_period == Period::Day) {
        output << boost::gregorian::to_iso_extended_string(m_date);
    } else if (m_period == Period::Month) {
        output << static_cast<int>(m_date.month()) << '/' << m_date.year();
    } else {
        output << m_date.year();
    }
    return output.str();
}

void ReportScreen::refresh() {
    NexoBarImp app;
    m_tickets.clear();
    m_cashSessions.clear();
    if (app->database()) {
        m_cashSessions = app->database()->selectAll<nexobar::CashSession>();
        for (const auto& ticket : app->database()->selectAll<nexobar::Ticket>())
            if (includes(ticket)) m_tickets.push_back(ticket);
    }
    m_periodLabel->text = app->text("reports.period");
    replaceToken(m_periodLabel->text, "{period}", periodLabel());
    m_summary->text = reportText();
}

std::string ReportScreen::reportText() const {
    NexoBarImp app;
    if (!app->database()) return {};
    std::vector<nexobar::Payment> payments;
    for (const auto& payment: app->database()->selectAll<nexobar::Payment>())
        if (includes(payment.createdAt)) payments.push_back(payment);
    std::vector<nexobar::CashSession> sessions;
    for (const auto& session: m_cashSessions)
        if (includes(session.openedAt)) sessions.push_back(session);
    return DailyCloseReceipt::buildReport(*app->database(), m_tickets, payments, sessions, periodLabel(),
                                          app->business().tradeName, *app->i18n());
}

void ReportScreen::print() {
    NexoBarImp app;
    std::string error;
    const bool printed = TicketPrinter{}.printText(reportText(), error, *app->i18n());
    m_status->text = printed ? app->text("reports.printed") : app->text("reports.print_failed") + error;
    m_status->color = printed ? Color{126, 225, 185, 255} : Color{255, 145, 135, 255};
}

void ReportScreen::closeAndPrint() {
    NexoBarImp app;
    if (m_period != Period::Current && m_period != Period::Day) return;
    if (m_period == Period::Current) m_date = boost::gregorian::day_clock::local_day();
    try {
        const auto closed = DailyCloseReceipt::close(*app->database(), m_date, m_userId);
        if (closed == 0) {
            m_status->text = app->text("reports.already_closed");
            m_status->color = Color{126, 225, 185, 255};
            refresh();
            return;
        }
        std::string error;
        const bool printed = DailyCloseReceipt::print(*app->database(), m_date,
            app->business().tradeName, *app->i18n(), error);
        m_status->text = printed ? app->text("reports.close_printed")
                                 : app->text("reports.close_without_print");
        replaceToken(m_status->text, "{count}", std::to_string(closed));
        m_status->color = Color{126, 225, 185, 255};
    } catch (const std::exception& error) {
        m_status->text = app->userError(error);
        m_status->color = Color{255, 145, 135, 255};
    }
    refresh();
}

void ReportScreen::exportFiscal(const nexobar::FiscalExportFormat format) {
    NexoBarImp app;
    try {
        if (m_period == Period::Year) throw nexobar::HospitalityError("errors.fiscal_export_day_month_only");
        auto from = m_period == Period::Current ? boost::gregorian::day_clock::local_day() : m_date;
        auto through = from;
        if (m_period == Period::Month) {
            from = boost::gregorian::date(m_date.year(), m_date.month(), 1);
            through = from.end_of_month();
        }
        nexobar::FiscalService fiscal(*app->database(), [app](const std::string_view key) {
            return app->i18n()->get(key);
        });
        const auto path = fiscal.exportPeriod(from, through, format);
        m_status->text = app->text("reports.exported") + path.string();
        m_status->color = Color{126, 225, 185, 255};
    } catch (const std::exception& error) {
        m_status->text = app->text(error.what());
        m_status->color = Color{255, 145, 135, 255};
    }
}

void ReportScreen::exportDeclaration() {
    NexoBarImp app;
    try {
        const auto path = nexobar::FiscalService::exportResponsibleDeclaration(app->business());
        m_status->text = app->text("reports.exported") + path.string();
        m_status->color = Color{126, 225, 185, 255};
    } catch (const std::exception& error) {
        m_status->text = app->text(error.what());
        m_status->color = Color{255, 145, 135, 255};
    }
}

void ReportScreen::verifyFiscal() {
    NexoBarImp app;
    try {
        nexobar::FiscalService fiscal(*app->database(), [app](const std::string_view key) {
            return app->i18n()->get(key);
        });
        if (!nexobar::FiscalService::signingConfigured())
            throw nexobar::HospitalityError("errors.fiscal_certificate_missing");
        const auto records = fiscal.verifyChain(true);
        const auto events = fiscal.verifyEventChain(true);
        if (!records.valid)
            throw nexobar::HospitalityError(records.error == "errors.fiscal_signature_missing"
                                                ? records.error : "errors.fiscal_chain_invalid");
        if (!events.valid)
            throw nexobar::HospitalityError(events.error == "errors.fiscal_signature_missing"
                                                ? events.error : "errors.fiscal_event_chain_invalid");
        m_status->text = app->text("reports.fiscal_verified");
        m_status->color = Color{126, 225, 185, 255};
    } catch (const std::exception& error) {
        m_status->text = app->text(error.what());
        m_status->color = Color{255, 145, 135, 255};
    }
}

void ReportScreen::layout() {
    const float unit = std::max(1.f, std::min(m_area.width, m_area.height));
    m_root->content.padding(LayoutInsets::all(std::clamp(unit * .035f, 14.f, 30.f))).gap(10.f);
    m_root->area(m_area);
    m_title->fontSize = std::clamp(unit * .065f, 27.f, 42.f);
    m_periodLabel->fontSize = m_status->fontSize = std::clamp(unit * .027f, 15.f, 19.f);
    m_summary->fontSize = std::clamp(unit * .025f, 14.f, 18.f);
}

void ReportScreen::update(const float deltaSeconds) {
    layout();
    m_root->update(deltaSeconds);
}

void ReportScreen::draw() {
    layout();
    m_root->draw();
}
