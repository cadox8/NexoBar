#include "TicketCancellationScreen.h"

#include "../../../NexoBar.h"
#include "../../../fiscal/FiscalService.h"
#include "database/hospitality/Money.h"
#include "../../../reporting/CashDay.h"

#include <algorithm>

TicketCancellationScreen::TicketCancellationScreen(const nexobar::UserId userId)
    : m_root(std::make_shared<ScrollableBlock>(LayoutAxis::Vertical, Color{24,31,42,255})),
      m_title(std::make_shared<StaticText>()), m_help(std::make_shared<StaticText>()),
      m_status(std::make_shared<StaticText>()), m_tickets(std::make_shared<DropdownComponent>()),
      m_reason(std::make_shared<InputText>()), m_cancel(std::make_shared<Button>()), m_userId(userId) {
    NexoBarImp app;
    m_font = app->defaultFont();
    for (const auto& text : {m_title, m_help, m_status}) {
        text->font = m_font; text->wrap = true; text->color = RAYWHITE;
    }
    m_title->text = app->text("cancellation.title");
    m_help->text = app->text("cancellation.help");
    m_status->color = Color{126,225,185,255};
    m_tickets->font = m_font;
    m_reason->font = m_font;
    m_reason->placeholder = app->text("cancellation.reason");
    m_reason->maxLength = 250;
    m_cancel->font = m_font;
    m_cancel->text = app->text("cancellation.action");
    m_cancel->background = Color{151,55,61,255};
    m_cancel->onClick([this] { cancelSelected(); });
    m_root->content.add(m_title, LayoutSize::fill(), LayoutSize::px(60));
    m_root->content.add(m_help, LayoutSize::fill(), LayoutSize::px(72));
    m_root->content.add(m_tickets, LayoutSize::fill(), LayoutSize::px(52));
    m_root->content.add(m_reason, LayoutSize::fill(), LayoutSize::px(58));
    m_root->content.add(m_cancel, LayoutSize::fill(), LayoutSize::px(54));
    m_root->content.add(m_status, LayoutSize::fill(), LayoutSize::px(72));
    refresh();
}

void TicketCancellationScreen::refresh() {
    NexoBarImp app;
    std::vector<DropdownOption> options;
    if (app->database()) {
        const auto cashSessions = app->database()->selectAll<nexobar::CashSession>();
        const auto tickets = app->database()->query<nexobar::Ticket>()
            .orderBy(Nearon::col<&nexobar::Ticket::id>.desc()).all();
        for (const auto& ticket : tickets) {
            const bool cancelled = app->database()->count<nexobar::FiscalRecord>(
                Nearon::col<&nexobar::FiscalRecord::ticketId> == ticket.id &&
                Nearon::col<&nexobar::FiscalRecord::type> == nexobar::FiscalRecordType::Cancellation) != 0;
            if (!nexobar::isTicketCashDayClosed(cashSessions, ticket) && !cancelled)
                options.push_back({ticket.fullNumber + " · " + nexobar::formatMoney(ticket.total, *app->i18n()),
                                   std::to_string(ticket.id.get())});
        }
    }
    m_tickets->setOptions(std::move(options), false);
    if (!m_tickets->options.empty()) m_tickets->select(0);
    m_cancel->enabled = !m_tickets->options.empty();
}

void TicketCancellationScreen::cancelSelected() {
    NexoBarImp app;
    const auto* selected = m_tickets->selected();
    if (!selected) return;
    if (m_reason->text.empty()) {
        m_status->text = app->text("errors.cancellation_reason_required");
        m_status->color = Color{255,145,135,255};
        return;
    }
    try {
        nexobar::FiscalService service(*app->database(), [app](const std::string_view key) {
            return app->i18n()->get(key);
        });
        service.cancel(app->business(), nexobar::TicketId{std::stoll(selected->value)}, m_userId, m_reason->text);
        m_status->text = app->text("cancellation.completed");
        m_status->color = Color{126,225,185,255};
        m_reason->setText({});
        refresh();
    } catch (const std::exception& error) {
        const std::string key = error.what();
        m_status->text = app->text(key);
        m_status->color = Color{255,145,135,255};
    }
}

void TicketCancellationScreen::layout() {
    const float unit = std::max(1.f, std::min(m_area.width, m_area.height));
    m_root->content.padding(LayoutInsets::all(std::clamp(unit * .04f, 18.f, 36.f))).gap(14.f);
    m_root->area(m_area);
    m_title->fontSize = std::clamp(unit * .06f, 28.f, 42.f);
    m_help->fontSize = m_status->fontSize = std::clamp(unit * .027f, 16.f, 20.f);
}

void TicketCancellationScreen::update(const float deltaSeconds) { layout(); m_root->update(deltaSeconds); }
void TicketCancellationScreen::draw() { layout(); m_root->draw(); if (m_tickets->open) m_tickets->drawOverlay(); }
