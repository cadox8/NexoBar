#include "OpenTabsPopupScreen.h"

#include "../../../NexoBar.h"
#include "database/Database.h"
#include "database/hospitality/HospitalityDatabase.h"
#include "database/hospitality/Money.h"
#include "printer/PrinterDiagnostics.h"
#include "template/TicketTemplate.h"
#include "time/AppTime.h"
#include "log/Log.h"

#include <raylib.h>

#include <algorithm>
#include <filesystem>
#include <ranges>

namespace {
    constexpr Color Accent{91, 207, 169, 255};
    constexpr Color Panel{27, 34, 46, 255};
}

OpenTabsPopupScreen::OpenTabsPopupScreen(std::shared_ptr<ProductState> state)
    : m_state(std::move(state)), m_close(std::make_shared<Button>()),
      m_newAccount(std::make_shared<Button>()), m_action(std::make_shared<Button>()),
      m_cancelCreate(std::make_shared<Button>()),
      m_removeLines(std::make_shared<Button>()), m_cancelAccount(std::make_shared<Button>()),
      m_confirmButton(std::make_shared<Button>()), m_cancelConfirm(std::make_shared<Button>()),
      m_name(std::make_shared<InputText>()) {
    NexoBarImp app;
    m_font = app->defaultFont();
    m_close->text = app->text("common.close_symbol");
    m_newAccount->text = app->text("open_tabs.new_account");
    m_cancelCreate->text = app->text("common.cancel");
    m_removeLines->text = app->text("open_tabs.remove_selected");
    m_cancelAccount->text = app->text("open_tabs.delete_account");
    m_confirmButton->text = app->text("open_tabs.confirm");
    m_cancelConfirm->text = app->text("common.cancel");
    for (const auto& button: {m_close, m_newAccount, m_action, m_cancelCreate,
                              m_removeLines, m_cancelAccount, m_confirmButton,
                              m_cancelConfirm}) {
        button->font = m_font;
        button->fontSize = 17;
        button->roundness = .14f;
    }
    m_close->background = Color{75, 48, 52, 255};
    m_newAccount->background = Color{52, 105, 170, 255};
    m_action->background = Color{35, 130, 100, 255};
    m_cancelCreate->background = Color{51, 62, 79, 255};
    m_removeLines->background = Color{130, 82, 48, 255};
    m_cancelAccount->background = Color{126, 52, 58, 255};
    m_confirmButton->background = Color{126, 52, 58, 255};
    m_cancelConfirm->background = Color{51, 62, 79, 255};
    m_close->onClick([this] { close(); });
    m_newAccount->onClick([this] { showCreate(); });
    m_removeLines->onClick([this] {
        if (!m_selectedLines.empty()) m_confirm = ConfirmAction::RemoveLines;
    });
    m_cancelAccount->onClick([this] {
        if (m_selected) m_confirm = ConfirmAction::CancelAccount;
    });
    m_confirmButton->onClick([this] { confirmAction(); });
    m_cancelConfirm->onClick([this] { m_confirm = ConfirmAction::None; });
    m_cancelCreate->onClick([this] {
        m_mode = m_returnMode;
        m_error.clear();
    });
    m_action->onClick([this] {
        if (m_mode == Mode::Create) createAccount();
        else if (m_mode == Mode::Add && m_selected) addCurrentTo(*m_selected);
        else if (m_mode == Mode::Browse) printSelected();
    });
    m_name->font = m_font;
    m_name->fontSize = 23;
    m_name->maxLength = 60;
    m_name->placeholder = app->text("open_tabs.name_placeholder");
    m_keyboard = std::make_shared<VirtualKeyboard>([this] { return m_name; });
    m_keyboard->font = m_font;
    m_keyboard->alwaysVisible = true;
}

void OpenTabsPopupScreen::showAccounts() {
    m_mode = Mode::Browse;
    m_visible = true;
    m_confirm = ConfirmAction::None;
    m_error.clear();
    reload();
}

void OpenTabsPopupScreen::showAddCurrent() {
    NexoBarImp app;
    m_mode = Mode::Add;
    m_visible = true;
    m_confirm = ConfirmAction::None;
    m_error.clear();
    if (m_state->selectedOrder.empty())
        m_error = app->text("open_tabs.empty_ticket");
    reload();
}

void OpenTabsPopupScreen::close() {
    m_visible = false;
    m_confirm = ConfirmAction::None;
    m_selectedLines.clear();
    m_name->focused = false;
}

void OpenTabsPopupScreen::reload() {
    NexoBarImp app;
    m_accounts = app->database()->query<nexobar::ConsumptionList>()
        .where(Nearon::col<&nexobar::ConsumptionList::status> == nexobar::ListStatus::Open)
        .orderBy(Nearon::col<&nexobar::ConsumptionList::openedAt>.asc()).all();
    m_accountButtons.clear();
    for (const auto& account: m_accounts) {
        auto button = std::make_shared<Button>(account.label + "  " + m_state->formatPrice(account.total));
        button->font = m_font;
        button->fontSize = 17;
        button->background = Color{42, 51, 66, 255};
        button->hoverBackground = Color{54, 68, 87, 255};
        const auto id = account.id;
        button->onClick([this, id] { select(id); });
        m_accountButtons.push_back(std::move(button));
    }
    const bool selectionStillExists = m_selected && std::ranges::any_of(
        m_accounts, [this](const auto& row) { return row.id == *m_selected; });
    if (!selectionStillExists) m_selected.reset();
    if (!m_selected && !m_accounts.empty()) m_selected = m_accounts.front().id;
    if (m_selected) select(*m_selected); else m_lines.clear();
}

void OpenTabsPopupScreen::select(const nexobar::ConsumptionListId id) {
    NexoBarImp app;
    m_selected = id;
    m_selectedLines.clear();
    m_lines = app->database()->query<nexobar::ListLine>()
        .where(Nearon::col<&nexobar::ListLine::listId> == id)
        .orderBy(Nearon::col<&nexobar::ListLine::addedAt>.asc()).all();
    m_detailScroll = 0.f;
}

void OpenTabsPopupScreen::showCreate() {
    m_returnMode = m_mode;
    m_mode = Mode::Create;
    m_name->setText("");
    m_name->focused = true;
    m_error.clear();
}

void OpenTabsPopupScreen::createAccount() {
    NexoBarImp app;
    std::string name = m_name->text;
    const auto begin = name.find_first_not_of(" \t\r\n");
    const auto end = name.find_last_not_of(" \t\r\n");
    name = begin == std::string::npos ? "" : name.substr(begin, end - begin + 1);
    if (name.empty()) {
        m_error = app->text("open_tabs.name_required");
        return;
    }
    try {
        nexobar::ConsumptionList account;
        account.label = name;
        account.customerName = name;
        account.openedBy = nexobar::UserId{m_state->userId};
        account.openedAt = AppTime::UTCNow();
        account.id = app->database()->insert(account);
        nexobar::audit(*app->database(), account.openedBy, "LIST_OPENED", "lists",
                       account.id.get(), name);
        if (m_returnMode == Mode::Add && !m_state->selectedOrder.empty()) {
            addCurrentTo(account.id);
            return;
        }
        m_mode = m_returnMode;
        reload();
        select(account.id);
    } catch (const std::exception& error) {
        m_error = app->userError(error);
    }
}

void OpenTabsPopupScreen::addCurrentTo(const nexobar::ConsumptionListId id) {
    NexoBarImp app;
    if (m_state->selectedOrder.empty()) {
        m_error = app->text("open_tabs.empty_ticket");
        return;
    }
    try {
        app->database()->transaction([&] {
            auto account = app->database()->get<nexobar::ConsumptionList>(id);
            if (account.status != nexobar::ListStatus::Open)
                throw nexobar::HospitalityError("errors.list_not_open");
            nexobar::Cents added = 0;
            for (const std::size_t index: m_state->selectedOrder) {
                if (index >= m_state->products.size() || index >= m_state->quantities.size() ||
                    m_state->quantities[index] <= 0) continue;
                const auto& product = m_state->products[index];
                const auto tax = std::ranges::find(m_state->taxes, product.taxId,
                                                   &ProductState::Tax::id);
                if (tax == m_state->taxes.end())
                    throw nexobar::HospitalityError("errors.product_tax_missing");
                nexobar::ListLine line;
                line.listId = id;
                if (product.id > 0) line.productId = nexobar::ProductId{product.id};
                line.description = product.name;
                line.quantity = static_cast<nexobar::QuantityMilli>(m_state->quantities[index]) * 1000;
                line.unitPrice = product.priceMinor;
                line.taxId = nexobar::TaxId{tax->id};
                line.taxRate = static_cast<nexobar::BasisPoints>(tax->rate);
                line.total = nexobar::lineTotal(line.unitPrice, line.quantity);
                line.addedBy = nexobar::UserId{m_state->userId};
                line.addedAt = AppTime::UTCNow();
                line.id = app->database()->insert(line);
                added += line.total;
            }
            account.total += added;
            app->database()->update(account);
            nexobar::audit(*app->database(), nexobar::UserId{m_state->userId},
                           "TICKET_ADDED_TO_LIST", "lists", id.get());
        });
        m_state->clearOrder();
        close();
    } catch (const std::exception& error) {
        m_error = app->userError(error);
    }
}

std::vector<nexobar::SaleItem> OpenTabsPopupScreen::selectedSaleItems() const {
    NexoBarImp app;
    std::vector<nexobar::SaleItem> items;
    items.reserve(m_lines.size());
    for (const auto& line: m_lines) {
        const auto tax = app->database()->get<nexobar::Tax>(line.taxId);
        nexobar::SaleItem item;
        item.productId = line.productId;
        item.description = line.description;
        item.quantity = line.quantity;
        item.unitPrice = line.unitPrice;
        item.taxRate = line.taxRate;
        item.taxCode = tax.taxCode;
        item.regimeCode = tax.regimeCode;
        item.operationCode = tax.operationCode;
        items.push_back(std::move(item));
    }
    return items;
}

void OpenTabsPopupScreen::printSelected() {
    if (!m_selected || m_lines.empty()) return;
    NexoBarImp app;
    try {
        nexobar::SaleService service(*app->database(), [app](const std::string_view key) {
            return app->i18n()->get(key);
        });
        const auto sale = service.issue(app->business(), nexobar::UserId{m_state->userId},
                                        selectedSaleItems(), m_selected);
        const auto templatePath = std::filesystem::path(GetApplicationDirectory()) /
                                  "resources" / "tickets" / "ticket.tpl";
        const auto result = TicketTemplate::printFile(
            templatePath.string(), sale.ticket, sale.lines,
            app->business().tradeName.empty() ? app->business().legalName : app->business().tradeName,
            *app->i18n(), {}, PrinterDiagnostics::detectedReceiptLayout());
        if (!result.printed)
            NEXO_ERROR(LogModule::Printer) << "Ticket print failed: " << result.error;
        reload();
        if (m_accounts.empty()) close();
    } catch (const std::exception& error) {
        m_error = app->userError(error);
    }
}

void OpenTabsPopupScreen::removeSelectedLines() {
    if (!m_selected || m_selectedLines.empty()) return;
    NexoBarImp app;
    try {
        const std::vector<nexobar::ListLineId> lines(m_selectedLines.begin(),
                                                     m_selectedLines.end());
        nexobar::removeListLines(*app->database(), *m_selected, lines,
                                 nexobar::UserId{m_state->userId});
        const auto selected = *m_selected;
        reload();
        if (std::ranges::any_of(m_accounts,
                [selected](const auto& account) { return account.id == selected; }))
            select(selected);
        m_error.clear();
    } catch (const std::exception& error) {
        m_error = app->userError(error);
    }
}

void OpenTabsPopupScreen::cancelSelectedAccount() {
    if (!m_selected) return;
    NexoBarImp app;
    try {
        nexobar::cancelList(*app->database(), *m_selected,
                            nexobar::UserId{m_state->userId});
        m_selected.reset();
        m_selectedLines.clear();
        m_error.clear();
        reload();
    } catch (const std::exception& error) {
        m_error = app->userError(error);
    }
}

void OpenTabsPopupScreen::confirmAction() {
    const auto action = m_confirm;
    m_confirm = ConfirmAction::None;
    if (action == ConfirmAction::RemoveLines) removeSelectedLines();
    else if (action == ConfirmAction::CancelAccount) cancelSelectedAccount();
}

void OpenTabsPopupScreen::layout() {
    const float w = std::min(1100.f, m_area.width * .94f);
    const float h = std::min(720.f, m_area.height * .92f);
    m_panel = {m_area.x + (m_area.width - w) * .5f,
               m_area.y + (m_area.height - h) * .5f, w, h};
    const float pad = 22.f;
    m_close->area({m_panel.x + w - 54.f, m_panel.y + 14.f, 38.f, 38.f});
    if (m_mode == Mode::Create) {
        m_name->area({m_panel.x + pad, m_panel.y + 86.f, w - pad * 2.f, 54.f});
        m_keyboard->area({m_panel.x + pad, m_panel.y + 164.f, w - pad * 2.f, h - 250.f});
        m_cancelCreate->area({m_panel.x + w - 254.f, m_panel.y + h - 62.f, 108.f, 42.f});
        m_action->area({m_panel.x + w - 136.f, m_panel.y + h - 62.f, 114.f, 42.f});
        return;
    }
    const float bodyY = m_panel.y + 72.f;
    const float bodyH = h - 148.f;
    m_left = {m_panel.x + pad, bodyY, w * .34f, bodyH};
    m_right = {m_left.x + m_left.width + 18.f, bodyY,
               w - m_left.width - pad * 2.f - 18.f, bodyH};
    const float rowH = 54.f;
    for (std::size_t i = 0; i < m_accountButtons.size(); ++i)
        m_accountButtons[i]->area({m_left.x, m_left.y + static_cast<float>(i) * (rowH + 7.f),
                                   m_left.width, rowH});
    const float footerY = m_panel.y + h - 62.f;
    const float leftGap = 8.f;
    m_newAccount->area({m_left.x, footerY, (m_left.width - leftGap) * .5f, 42.f});
    m_cancelAccount->area({m_left.x + (m_left.width + leftGap) * .5f, footerY,
                           (m_left.width - leftGap) * .5f, 42.f});
    m_removeLines->area({m_right.x, footerY, 210.f, 42.f});
    m_action->area({m_right.x + m_right.width - 210.f, footerY, 210.f, 42.f});
    const float confirmY = m_panel.y + h * .5f + 28.f;
    m_cancelConfirm->area({m_panel.x + w * .5f - 128.f, confirmY, 116.f, 42.f});
    m_confirmButton->area({m_panel.x + w * .5f + 12.f, confirmY, 116.f, 42.f});
}

void OpenTabsPopupScreen::update(const float deltaSeconds) {
    if (!m_visible) return;
    layout();
    if (m_confirm != ConfirmAction::None) {
        if (IsKeyPressed(KEY_ESCAPE)) {
            m_confirm = ConfirmAction::None;
            return;
        }
        m_confirmButton->update(deltaSeconds);
        m_cancelConfirm->update(deltaSeconds);
        return;
    }
    if (IsKeyPressed(KEY_ESCAPE)) { close(); return; }
    m_close->update(deltaSeconds);
    if (m_mode == Mode::Create) {
        m_action->text = NexoBarImp()->text("open_tabs.create");
        m_action->enabled = true;
        m_name->update(deltaSeconds);
        m_keyboard->update(deltaSeconds);
        m_cancelCreate->update(deltaSeconds);
        m_action->update(deltaSeconds);
        return;
    }
    for (const auto& button: m_accountButtons) button->update(deltaSeconds);
    m_newAccount->update(deltaSeconds);
    m_cancelAccount->enabled = m_selected.has_value() &&
        m_state->hasPermission("close_list");
    m_cancelAccount->update(deltaSeconds);
    m_removeLines->enabled = m_selected.has_value() && !m_selectedLines.empty() &&
        m_state->hasPermission("remove_line");
    m_removeLines->update(deltaSeconds);
    m_action->text = NexoBarImp()->text(m_mode == Mode::Add ? "open_tabs.add_to_account" : "lists.print");
    m_action->enabled = m_selected.has_value() &&
        (m_mode == Mode::Browse
             ? !m_lines.empty() && m_state->hasPermission("print_ticket") &&
                   m_state->hasPermission("close_list")
             : !m_state->selectedOrder.empty());
    m_action->update(deltaSeconds);
    if (m_selected && UIInput::pointerReleased() && !UIInput::clicksSuppressed() &&
        CheckCollisionPointRec(UIInput::pointerPosition(), m_right)) {
        float y = m_right.y + 18.f - m_detailScroll;
        for (const auto& line: m_lines) {
            const Rectangle row{m_right.x, y - 7.f, m_right.width, 34.f};
            if (CheckCollisionPointRec(UIInput::pointerPosition(), row)) {
                if (m_selectedLines.contains(line.id)) m_selectedLines.erase(line.id);
                else m_selectedLines.insert(line.id);
                break;
            }
            y += 38.f;
        }
    }
    if (CheckCollisionPointRec(GetMousePosition(), m_right))
        m_detailScroll = std::max(0.f, m_detailScroll - GetMouseWheelMove() * 28.f);
}

void OpenTabsPopupScreen::draw() {
    if (!m_visible) return;
    layout();
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{0, 0, 0, 175});
    DrawRectangleRounded(m_panel, .025f, 10, Panel);
    DrawRectangleRoundedLinesEx(m_panel, .025f, 10, 1.5f, Color{74, 91, 112, 255});
    NexoBarImp app;
    const std::string title = app->text(m_mode == Mode::Create ? "open_tabs.create_title" :
                                        (m_mode == Mode::Add ? "open_tabs.add_title" : "open_tabs.title"));
    DrawTextEx(*m_font, title.c_str(), {m_panel.x + 22.f, m_panel.y + 20.f}, 25.f, 1.f, RAYWHITE);
    m_close->draw();
    if (m_mode == Mode::Create) {
        m_name->draw();
        m_keyboard->draw();
        m_cancelCreate->draw();
        m_action->draw();
    } else {
        DrawTextEx(*m_font, app->text("open_tabs.people").c_str(), {m_left.x, m_left.y - 28.f}, 18.f, 1.f, Color{190, 199, 212, 255});
        DrawTextEx(*m_font, app->text("open_tabs.breakdown").c_str(), {m_right.x, m_right.y - 28.f}, 18.f, 1.f, Color{190, 199, 212, 255});
        for (std::size_t i = 0; i < m_accountButtons.size(); ++i) {
            if (m_selected && m_accounts[i].id == *m_selected)
                m_accountButtons[i]->background = Color{52, 105, 120, 255};
            else m_accountButtons[i]->background = Color{42, 51, 66, 255};
            m_accountButtons[i]->draw();
        }
        if (m_accounts.empty())
            DrawTextEx(*m_font, app->text("open_tabs.none").c_str(), {m_left.x, m_left.y + 10.f}, 18.f, 1.f, Color{145, 155, 170, 255});
        DrawRectangleRounded(m_right, .02f, 6, Color{34, 42, 55, 255});
        BeginScissorMode(static_cast<int>(m_right.x), static_cast<int>(m_right.y),
                         static_cast<int>(m_right.width), static_cast<int>(m_right.height));
        float y = m_right.y + 18.f - m_detailScroll;
        for (const auto& line: m_lines) {
            if (m_selectedLines.contains(line.id)) {
                DrawRectangleRounded({m_right.x + 5.f, y - 7.f, m_right.width - 10.f, 32.f},
                                     .08f, 4, Color{48, 91, 91, 255});
                DrawRectangleRounded({m_right.x + 10.f, y + 1.f, 9.f, 16.f},
                                     .25f, 4, Accent);
            }
            const std::string quantity = line.quantity == 1000 ? "" : " x" + std::to_string(line.quantity / 1000);
            DrawTextEx(*m_font, (line.description + quantity).c_str(), {m_right.x + 26.f, y}, 19.f, 1.f, RAYWHITE);
            const std::string amount = m_state->formatPrice(line.total);
            const auto measured = MeasureTextEx(*m_font, amount.c_str(), 19.f, 1.f);
            DrawTextEx(*m_font, amount.c_str(), {m_right.x + m_right.width - measured.x - 18.f, y}, 19.f, 1.f, Accent);
            y += 38.f;
        }
        if (m_selected) {
            const auto account = std::ranges::find(m_accounts, *m_selected, &nexobar::ConsumptionList::id);
            if (account != m_accounts.end()) {
                y += 10.f;
                DrawLineEx({m_right.x + 18.f, y}, {m_right.x + m_right.width - 18.f, y}, 1.f, Color{82, 94, 110, 255});
                y += 16.f;
                const std::string total = app->text("receipt.total") + ": " + m_state->formatPrice(account->total);
                DrawTextEx(*m_font, total.c_str(), {m_right.x + 18.f, y}, 22.f, 1.f, Accent);
            }
        }
        EndScissorMode();
        m_newAccount->draw();
        m_cancelAccount->draw();
        m_removeLines->draw();
        m_action->draw();
    }
    if (!m_error.empty())
        DrawTextEx(*m_font, m_error.c_str(), {m_panel.x + 22.f, m_panel.y + m_panel.height - 91.f}, 16.f, 1.f, Color{255, 135, 125, 255});
    if (m_confirm != ConfirmAction::None) {
        const Rectangle dialog{m_panel.x + m_panel.width * .5f - 260.f,
                               m_panel.y + m_panel.height * .5f - 90.f, 520.f, 180.f};
        DrawRectangleRec(m_panel, Color{0, 0, 0, 145});
        DrawRectangleRounded(dialog, .06f, 8, Color{38, 46, 59, 255});
        DrawRectangleRoundedLinesEx(dialog, .06f, 8, 1.5f, Color{105, 116, 132, 255});
        const std::string message = app->text(
            m_confirm == ConfirmAction::RemoveLines
                ? "open_tabs.confirm_remove_lines"
                : "open_tabs.confirm_delete_account");
        const auto measured = MeasureTextEx(*m_font, message.c_str(), 19.f, 1.f);
        DrawTextEx(*m_font, message.c_str(),
                   {dialog.x + (dialog.width - measured.x) * .5f, dialog.y + 35.f},
                   19.f, 1.f, RAYWHITE);
        m_cancelConfirm->draw();
        m_confirmButton->draw();
    }
}
