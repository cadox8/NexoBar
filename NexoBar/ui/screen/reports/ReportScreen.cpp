#include "ReportScreen.h"
#include "../../../NexoBar.h"
#include "../../../printer/TicketPrinter.h"
#include "../../../reporting/DailyCloseReceipt.h"
#include "database/hospitality/Money.h"
#include "database/models/Payment.h"
#include <algorithm>
#include <sstream>

ReportScreen::ReportScreen(const nexobar::UserId userId, const bool canPrint, const bool canClose)
    : m_root(std::make_shared<ScrollableBlock>(LayoutAxis::Vertical, Color{24,31,42,255})),
      m_title(std::make_shared<StaticText>()), m_periodLabel(std::make_shared<StaticText>()),
      m_summary(std::make_shared<StaticText>()), m_status(std::make_shared<StaticText>()),
      m_date(boost::gregorian::day_clock::local_day()), m_userId(userId) {
    NexoBarImp app; m_font = app->defaultFont();
    for (const auto& text : {m_title,m_periodLabel,m_summary,m_status}) { text->font=m_font; text->color=RAYWHITE; text->wrap=true; }
    m_title->text=app->text("reports.title"); m_status->color=Color{126,225,185,255};
    const auto make=[&](const std::string& key, auto action){ auto b=std::make_shared<Button>(app->text(key),action); b->font=m_font; b->fontSize=17; return b; };
    m_day=make("reports.day",[this]{setPeriod(Period::Day);});
    m_month=make("reports.month",[this]{setPeriod(Period::Month);});
    m_year=make("reports.year",[this]{setPeriod(Period::Year);});
    m_previous=make("reports.previous",[this]{move(-1);}); m_next=make("reports.next",[this]{move(1);});
    m_print=make("reports.print",[this]{print();}); m_print->enabled=canPrint;
    m_close=make("reports.close_and_print",[this]{closeAndPrint();}); m_close->enabled=canClose;
    const auto row=[](std::initializer_list<ButtonPtr> buttons){ auto r=std::make_shared<BlockComponent>(LayoutAxis::Horizontal); r->content.gap(10); for(const auto& b:buttons)r->content.add(b,LayoutSize::fill(),LayoutSize::fill()); return r; };
    m_periodActions=row({m_day,m_month,m_year}); m_dateActions=row({m_previous,m_next}); m_printActions=row({m_print,m_close});
    m_root->content.add(m_title,LayoutSize::fill(),LayoutSize::px(58));
    m_root->content.add(m_periodActions,LayoutSize::fill(),LayoutSize::px(50));
    m_root->content.add(m_dateActions,LayoutSize::fill(),LayoutSize::px(50));
    m_root->content.add(m_periodLabel,LayoutSize::fill(),LayoutSize::px(42));
    m_root->content.add(m_printActions,LayoutSize::fill(),LayoutSize::px(52));
    m_root->content.add(m_summary,LayoutSize::fill(),LayoutSize::px(390));
    m_root->content.add(m_status,LayoutSize::fill(),LayoutSize::px(52)); refresh();
}
void ReportScreen::setPeriod(const Period p){m_period=p;m_close->visible=p==Period::Day;refresh();}
void ReportScreen::move(const int n){if(m_period==Period::Day)m_date+=boost::gregorian::days(n);else if(m_period==Period::Month)m_date+=boost::gregorian::months(n);else m_date+=boost::gregorian::years(n);refresh();}
bool ReportScreen::includes(const nexobar::Ticket& t)const{const auto d=t.issuedAt.date();if(m_period==Period::Day)return d==m_date;if(m_period==Period::Month)return d.year()==m_date.year()&&d.month()==m_date.month();return d.year()==m_date.year();}
std::string ReportScreen::periodLabel()const{std::ostringstream o;if(m_period==Period::Day)o<<boost::gregorian::to_iso_extended_string(m_date);else if(m_period==Period::Month)o<<static_cast<int>(m_date.month())<<'/'<<m_date.year();else o<<m_date.year();return o.str();}
void ReportScreen::refresh(){NexoBarImp app;m_tickets.clear();if(app->database())for(const auto&t:app->database()->selectAll<nexobar::Ticket>())if(includes(t))m_tickets.push_back(t);m_periodLabel->text=app->text("reports.period");replaceToken(m_periodLabel->text,"{period}",periodLabel());m_summary->text=reportText();}
std::string ReportScreen::reportText()const{NexoBarImp app;std::int64_t base=0,tax=0,total=0,cash=0,card=0;for(const auto&t:m_tickets){base+=t.taxBase;tax+=t.taxAmount;total+=t.total;}if(app->database())for(const auto&p:app->database()->selectAll<nexobar::Payment>()){nexobar::Ticket probe;probe.issuedAt=p.createdAt;if(!includes(probe))continue;(p.method==nexobar::PaymentMethod::Cash?cash:card)+=p.amount;}std::ostringstream o;o<<app->text("settings.report_header")<<'\n'<<periodLabel()<<'\n'<<app->text("settings.report_tickets")<<m_tickets.size()<<'\n'<<app->text("settings.report_tax_base")<<nexobar::formatMoney(base,*app->i18n())<<'\n'<<app->text("settings.report_tax")<<nexobar::formatMoney(tax,*app->i18n())<<'\n'<<app->text("settings.report_total")<<nexobar::formatMoney(total,*app->i18n())<<'\n'<<app->text("settings.report_cash")<<nexobar::formatMoney(cash,*app->i18n())<<'\n'<<app->text("settings.report_card")<<nexobar::formatMoney(card,*app->i18n())<<'\n'<<app->text("reports.separator")<<'\n';for(const auto&t:m_tickets)o<<t.fullNumber<<"  "<<nexobar::formatMoney(t.total,*app->i18n())<<'\n';return o.str();}
void ReportScreen::print(){NexoBarImp app;std::string error;bool ok=false;if(m_period==Period::Day)ok=DailyCloseReceipt::print(*app->database(),m_date,app->business().tradeName,*app->i18n(),false,m_userId,error);else{TicketPrinter p;ok=p.printText(reportText(),error,*app->i18n());}m_status->text=ok?app->text("reports.printed"):app->text("reports.print_failed")+error;m_status->color=ok?Color{126,225,185,255}:Color{255,145,135,255};}
void ReportScreen::closeAndPrint(){NexoBarImp app;std::string error;const bool ok=m_period==Period::Day&&DailyCloseReceipt::print(*app->database(),m_date,app->business().tradeName,*app->i18n(),true,m_userId,error);m_status->text=ok?app->text("reports.close_printed"):app->text("reports.print_failed")+error;m_status->color=ok?Color{126,225,185,255}:Color{255,145,135,255};refresh();}
void ReportScreen::layout(){const float u=std::max(1.f,std::min(m_area.width,m_area.height));m_root->content.padding(LayoutInsets::all(std::clamp(u*.035f,14.f,30.f))).gap(10);m_root->area(m_area);m_title->fontSize=std::clamp(u*.065f,27.f,42.f);m_periodLabel->fontSize=m_summary->fontSize=m_status->fontSize=std::clamp(u*.027f,15.f,19.f);}
void ReportScreen::update(const float dt){layout();m_root->update(dt);} void ReportScreen::draw(){layout();m_root->draw();}
