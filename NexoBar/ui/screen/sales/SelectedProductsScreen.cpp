#include "SelectedProductsScreen.h"

#include "../../../NexoBar.h"
#include "printer/PrinterDiagnostics.h"
#include "printer/TicketPrinter.h"
#include "template/TicketTemplate.h"

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iterator>
#include <memory>
#include <sstream>
#include <utility>

#include "log/Log.h"

namespace {
    constexpr Color Accent{.r = 91, .g = 207, .b = 169, .a = 255};

    std::string previewText(const std::string& bytes) {
        std::string result;
        for (std::size_t i = 0; i < bytes.size();) {
            const auto c = static_cast<unsigned char>(bytes[i]);
            if (c == 0x1B && i + 1 < bytes.size()) {
                const auto command = static_cast<unsigned char>(bytes[i + 1]);
                i += command == 'p' ? 5 : (command == 'a' ? 3 : 2);
                continue;
            }
            if (c == 0x1D && i + 1 < bytes.size()) {
                i += bytes[i + 1] == 'V' ? 3 : 2;
                continue;
            }
            if (c == '\n' || c == '\t' || c >= 0x20)
                result.push_back(bytes[i]);
            ++i;
        }
        return result;
    }
} // namespace

SelectedProductsScreen::SelectedProductsScreen(
    std::shared_ptr<ProductState> state,
    boost::signals2::signal<void(std::size_t)>& productSelected)
    : m_state(std::move(state)),
      m_items(std::make_shared<ScrollableBlock>(LayoutAxis::Vertical, Panel)),
      m_title(std::make_shared<StaticText>()),
      m_totalLabel(std::make_shared<StaticText>()),
      m_manualAmountLabel(std::make_shared<StaticText>()),
      m_printButton(std::make_shared<Button>()),
      m_drawerButton(std::make_shared<Button>()),
      m_addManualButton(std::make_shared<Button>()) {

    NexoBarImp app;

    m_font = app->defaultFont();
    m_decimalSeparator = app->text("number.decimal_separator", ",");
    m_title->text = app->text("lists.title", "Ticket");
    m_title->font = m_font;
    m_title->fontSize = 30;
    m_title->color = RAYWHITE;
    m_title->horizontalAlignment = TextHorizontalAlignment::Center;
    m_totalLabel->font = m_font;
    m_totalLabel->fontSize = 24;
    m_totalLabel->color = Accent;
    m_totalLabel->horizontalAlignment = TextHorizontalAlignment::Left;
    m_totalLabel->verticalAlignment = TextVerticalAlignment::Center;
    m_manualAmountLabel->font = m_font;
    m_manualAmountLabel->fontSize = 20;
    m_manualAmountLabel->color = RAYWHITE;
    m_manualAmountLabel->horizontalAlignment = TextHorizontalAlignment::Center;
    updateManualAmountDisplay();

    m_printButton->font = m_font;
    m_printButton->fontSize = 18;
    m_printButton->text = app->text("lists.print", "Imprimir");
    m_printButton->background = Color{170, 235, 190, 255};
    m_printButton->hoverBackground = Color{184, 245, 202, 255};
    m_printButton->pressedBackground = Color{150, 218, 172, 255};
    m_printButton->textColor = BLACK;
    m_printButton->onClick([&] { printTicket(); });

    m_drawerButton->font = m_font;
    m_drawerButton->fontSize = 16;
    m_drawerButton->text = app->text("lists.open_drawer", "");
    m_drawerButton->background = Color{225, 190, 125, 255};
    m_drawerButton->hoverBackground = Color{238, 207, 148, 255};
    m_drawerButton->pressedBackground = Color{204, 166, 101, 255};
    m_drawerButton->textColor = BLACK;
    m_drawerButton->onClick([app] {
        std::string error;
        TicketPrinter printer;
        if (!printer.openDrawer(error, *app->i18n()))
            NEXO_ERROR(LogModule::Printer) << "Could not open cash drawer: " << error;
    });

    m_addManualButton->font = m_font;
    m_addManualButton->fontSize = 18;
    m_addManualButton->text = app->text("lists.add_manual", "Añadir importe");
    m_addManualButton->background = Color{.r = 255, .g = 218, .b = 142, .a = 255};
    m_addManualButton->hoverBackground =
            Color{.r = 255, .g = 228, .b = 162, .a = 255};
    m_addManualButton->pressedBackground =
            Color{.r = 238, .g = 198, .b = 116, .a = 255};
    m_addManualButton->textColor = BLACK;
    m_addManualButton->onClick([&] { appendManualAmount(); });

    const std::string backspace = app->text("lists.backspace", "DEL");
    const std::string decimalSeparator = m_decimalSeparator;
    const std::string keypadLabels[] = {
        "1", "2", "3", "4",
        "5", "6", "7", "8",
        "9", backspace, "0", decimalSeparator
    };
    m_numericButtons.reserve(std::size(keypadLabels));
    for (std::size_t index = 0; index < std::size(keypadLabels); ++index) {
        const std::string label = keypadLabels[index];
        const std::string key =
                index == 9 ? "DEL" : (index == 11 ? "DECIMAL" : label);
        auto button = std::make_shared<Button>(label);
        button->font = m_font;
        button->fontSize = 21;
        button->background = Color{176, 210, 245, 255};
        button->hoverBackground = Color{190, 222, 252, 255};
        button->pressedBackground = Color{154, 192, 232, 255};
        button->textColor = BLACK;
        button->padding = LayoutInsets::symmetric(2, 2);
        button->onClick([this, key] { appendManualKey(key); });
        m_numericButtons.push_back(std::move(button));
    }

    m_productConnection = productSelected.connect(
        [state = m_state](const std::size_t index) { state->addProduct(index); });
    m_orderConnection = m_state->orderChanged.connect([this] { rebuildRows(); });
    rebuildRows();
}

void SelectedProductsScreen::layout() {
    NexoBarImp app;
    const float width = m_area.width;
    const float height = m_area.height;
    const float unit = std::max(1.f, std::min(width, height));
    const float inset = unit * .035f;
    const float listX = m_area.x + inset;
    const float listWidth = std::max(0.f, width - inset * 2.f);
    const float bottom = m_area.y + height - inset;
    const float totalHeight = height * .095f;
    const float totalY = bottom - totalHeight;
    const float keypadHeight = height * .34f;
    const float keyGap = unit * .014f;
    const float amountHeight = keypadHeight * .18f;
    const float addHeight = keypadHeight * .18f;
    const float keysHeight =
            std::max(0.f, keypadHeight - amountHeight - addHeight - keyGap * 5.f);
    const float keyHeight = keysHeight / 4.f;
    const float keyWidth = std::max(0.f, (listWidth - keyGap * 2.f) / 3.f);
    const float keypadY = totalY - keypadHeight;
    const float headingHeight = height * .1f;
    const float amountY = keypadY;
    const float keysY = amountY + amountHeight + keyGap;
    const float addY = keysY + keyHeight * 4.f + keyGap * 4.f;
    const float listTop = m_area.y + headingHeight + keyGap;
    const float fontScale = std::clamp(unit / 420.f, .72f, 1.45f);

    m_title->fontSize = std::clamp(26.f * fontScale, 16.f, 36.f);
    m_manualAmountLabel->fontSize = std::clamp(27.f * fontScale, 20.f, 38.f);
    m_addManualButton->fontSize =
            static_cast<int>(std::clamp(17.f * fontScale, 13.f, 23.f));
    m_printButton->fontSize = m_addManualButton->fontSize;
    m_drawerButton->fontSize = m_addManualButton->fontSize;
    for (const auto& button: m_numericButtons)
        button->fontSize =
                static_cast<int>(std::clamp(20.f * fontScale, 13.f, 27.f));
    m_title->area(
        {listX, m_area.y + inset * .4f, listWidth, headingHeight - inset * .4f});
    m_manualAmountLabel->area({listX, amountY, listWidth, amountHeight});
    for (std::size_t index = 0; index < m_numericButtons.size(); ++index) {
        const float x = listX + static_cast<float>(index % 3) * (keyWidth + keyGap);
        const float y =
                keysY + static_cast<float>(index / 3) * (keyHeight + keyGap);
        m_numericButtons[index]->area({x, y, keyWidth, keyHeight});
    }
    m_addManualButton->area(
        {.x = listX, .y = addY, .width = listWidth, .height = addHeight});
    m_items->area({
        .x = listX,
        .y = listTop,
        .width = listWidth,
        .height = std::max(0.f, keypadY - listTop - keyGap)
    });

    m_totalLabel->text = (app ? app->text("receipt.total", "TOTAL") : "TOTAL") +
                         ": " + m_state->formatPrice(m_state->total());
    m_totalLabel->fontSize = std::clamp(25.f * fontScale, 20.f, 36.f);
    m_totalLabel->area({
        .x = listX + unit * .018f,
        .y = totalY,
        .width = listWidth * .46f - unit * .036f,
        .height = totalHeight
    });
    m_printButton->area({
        .x = listX + listWidth * .75f,
        .y = totalY + totalHeight * .12f,
        .width = listWidth * .25f,
        .height = totalHeight * .76f
    });
    m_drawerButton->area({
        .x = listX + listWidth * .50f,
        .y = totalY + totalHeight * .12f,
        .width = listWidth * .23f,
        .height = totalHeight * .76f
    });
}

void SelectedProductsScreen::updateManualAmountDisplay() {
    NexoBarImp app;
    const std::string amount = m_manualAmountInput.empty()
                                   ? "0" + m_decimalSeparator + "00"
                                   : m_manualAmountInput;
    m_manualAmountLabel->text =
            app
                ? app->text("lists.amount_prefix", "Importe: ") + amount
                : "Importe: " + amount;
}

void SelectedProductsScreen::appendManualKey(const std::string& key) {
    if (key == "DEL") {
        if (!m_manualAmountInput.empty())
            m_manualAmountInput.pop_back();
    } else if (key == "DECIMAL") {
        if (m_manualAmountInput.find(m_decimalSeparator) == std::string::npos)
            m_manualAmountInput = m_manualAmountInput.empty()
                                      ? "0" + m_decimalSeparator
                                      : m_manualAmountInput + m_decimalSeparator;
    } else if (key.size() == 1 && key.front() >= '0' && key.front() <= '9') {
        const auto separator = m_manualAmountInput.find(m_decimalSeparator);
        const std::size_t fractionalDigits =
                separator == std::string::npos
                    ? 0
                    : m_manualAmountInput.size() - separator - 1;
        const std::size_t wholeDigits =
                separator == std::string::npos ? m_manualAmountInput.size() : separator;
        if ((separator == std::string::npos && wholeDigits < 12) ||
            (separator != std::string::npos && fractionalDigits < 2))
            m_manualAmountInput += key;
    }
    updateManualAmountDisplay();
}

void SelectedProductsScreen::appendManualAmount() {
    NexoBarImp app;
    if (m_manualAmountInput.empty() ||
        m_manualAmountInput == "0" + m_decimalSeparator)
        return;
    const std::size_t separator = m_manualAmountInput.find(m_decimalSeparator);
    const std::size_t wholeLength =
            separator == std::string::npos ? m_manualAmountInput.size() : separator;
    std::int64_t whole = 0;
    for (std::size_t index = 0; index < wholeLength; ++index)
        whole = whole * 10 + (m_manualAmountInput[index] - '0');
    std::int64_t cents = 0;
    if (separator != std::string::npos) {
        for (std::size_t index = separator + 1; index < m_manualAmountInput.size();
             ++index)
            cents = cents * 10 + (m_manualAmountInput[index] - '0');
        if (m_manualAmountInput.size() - separator == 2)
            cents *= 10;
    }
    const std::int64_t price = whole * 100 + cents;
    if (price <= 0)
        return;
    const std::string name =
            app ? app->text("lists.manual_amount", "Manual") : "Manual";
    std::int64_t manualId = -1;
    for (const auto& product: m_state->products)
        if (product.id <= manualId)
            manualId = product.id - 1;
    const std::int64_t taxId =
            m_state->taxes.empty() ? 0 : m_state->taxes.front().id;
    m_state->products.push_back({manualId, -1, name, price, taxId, true});
    m_state->quantities.push_back(0);
    m_state->addProduct(m_state->products.size() - 1);
    m_manualAmountInput.clear();
    updateManualAmountDisplay();
}

void SelectedProductsScreen::printTicket() {
    NexoBarImp app;

    try {
        if (!m_pendingPrint) {
            std::vector<nexobar::SaleItem> items;
            items.reserve(m_state->selectedOrder.size());
            for (const std::size_t index: m_state->selectedOrder) {
                if (index >= m_state->products.size() ||
                    index >= m_state->quantities.size() ||
                    m_state->quantities[index] <= 0)
                    continue;
                const auto& product = m_state->products[index];
                const auto tax = std::ranges::find(m_state->taxes, product.taxId,
                                                   &ProductState::Tax::id);
                if (tax == m_state->taxes.end())
                    throw std::runtime_error("No se encuentra el impuesto de " +
                                             product.name + ".");
                nexobar::SaleItem item;
                if (product.id >= 0)
                    item.productId = nexobar::ProductId{product.id};
                item.description = product.name;
                item.quantity =
                        static_cast<nexobar::QuantityMilli>(m_state->quantities[index]) *
                        1000;
                item.unitPrice = product.priceMinor;
                item.taxRate = static_cast<nexobar::BasisPoints>(tax->rate);
                items.push_back(std::move(item));
            }
            if (items.empty())
                return;
            nexobar::SaleService service(*app->database());
            m_pendingPrint = service.issue(app->business(),
                                           nexobar::UserId{m_state->userId}, items);
            // The sale is already immutable at this point. Clear the editable order
            // even if the physical printer fails.
            m_state->clearOrder();
        }

        const std::filesystem::path templatePath =
                std::filesystem::path(GetApplicationDirectory()) / "resources" /
                "tickets" / "ticket.tpl";
        const auto result = TicketTemplate::printFile(
            templatePath.string(), m_pendingPrint->ticket, m_pendingPrint->lines,
            m_state->businessName, *app->i18n(), {},
            PrinterDiagnostics::detectedReceiptLayout());
        if (app->ticketDebugMode()) {
            m_ticketPreview = previewText(result.rendered);
            m_ticketPreviewScroll = 0.f;
            m_ticketPreviewOpen = true;
        }
        if (!result.printed) {
            NEXO_ERROR(LogModule::Printer) << "Ticket print failed: " << result.error;
            return;
        }
        m_pendingPrint.reset();
    } catch (const std::exception& error) {
        NEXO_ERROR(LogModule::Core)
                << "Could not generate the receipt: " << error.what();
    }
}

void SelectedProductsScreen::rebuildRows() {
    NexoBarImp app;
    m_items->content.clear();
    for (const std::size_t index: m_state->selectedOrder) {
        if (index >= m_state->products.size() ||
            index >= m_state->quantities.size())
            continue;
        const int quantity = m_state->quantities[index];
        if (quantity <= 0)
            continue;
        const auto& product = m_state->products[index];
        const float unit = std::max(1.f, std::min(m_area.width, m_area.height));
        const float fontSize = std::clamp(unit * .044f, 14.f, 20.f);
        const float gap = std::max(2.f, unit * .012f);
        auto row = std::make_shared<BlockComponent>(LayoutAxis::Horizontal,
                                                    Color{36, 44, 57, 255});
        row->roundness = .10f;
        row->content.gap(gap).padding(LayoutInsets::symmetric(gap, gap * .45f));
        std::string quantityFormat =
                app
                    ? app->text("lists.quantity_format", "{name} (x{quantity})")
                    : "{name} (x{quantity})";
        const auto replace = [&quantityFormat](const std::string& key,
                                               const std::string& value) {
            const auto pos = quantityFormat.find(key);
            if (pos != std::string::npos)
                quantityFormat.replace(pos, key.size(), value);
        };
        replace("{name}", product.name);
        replace("{quantity}", std::to_string(quantity));
        auto label = std::make_shared<StaticText>(quantityFormat);
        label->font = m_font;
        label->fontSize = fontSize;
        label->color = RAYWHITE;
        label->horizontalAlignment = TextHorizontalAlignment::Left;
        label->verticalAlignment = TextVerticalAlignment::Center;
        label->wrap = false;
        label->ellipsis = true;
        auto price = std::make_shared<StaticText>(
            m_state->formatPrice(product.priceMinor * quantity));
        price->font = m_font;
        price->color = Accent;
        price->horizontalAlignment = TextHorizontalAlignment::Right;
        price->verticalAlignment = TextVerticalAlignment::Center;
        auto removeButton = std::make_shared<Button>("X");
        removeButton->font = m_font;
        removeButton->fontSize = static_cast<int>(std::max(15.f, fontSize - 2.f));
        removeButton->padding = LayoutInsets::symmetric(gap * .4f, gap * .3f);
        removeButton->background = Color{71, 48, 52, 255};
        removeButton->hoverBackground = removeButton->background;
        removeButton->pressedBackground = Color{90, 58, 62, 255};
        removeButton->textColor = RAYWHITE;
        removeButton->onClick(
            [state = m_state, index] { state->removeProduct(index); });
        price->fontSize = std::max(14.f, fontSize - 2.f);
        row->add(label, LayoutSize::fill(), LayoutSize::fill());
        row->add(price, LayoutSize::px(std::clamp(unit * .19f, 60.f, 95.f)),
                 LayoutSize::fill());
        row->add(removeButton, LayoutSize::px(std::clamp(unit * .105f, 32.f, 40.f)),
                 LayoutSize::fill());
        m_items->content.add(row, LayoutSize::fill(),
                             LayoutSize::px(std::clamp(unit * .125f, 42.f, 60.f)));
    }
}

void SelectedProductsScreen::update(const float deltaSeconds) {
    if (m_ticketPreviewOpen) {
        if (IsKeyPressed(KEY_ESCAPE))
            m_ticketPreviewOpen = false;
        const float w = std::min(640.f, m_area.width * .88f);
        const float h = std::min(620.f, m_area.height * .88f);
        const Rectangle panel{
            m_area.x + (m_area.width - w) * .5f,
            m_area.y + (m_area.height - h) * .5f, w, h
        };
        const Rectangle close{
            panel.x + panel.width - 48.f, panel.y + 10.f, 36.f,
            36.f
        };
        const Vector2 mouse = GetMousePosition();
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
            CheckCollisionPointRec(mouse, close))
            m_ticketPreviewOpen = false;
        else if (CheckCollisionPointRec(mouse,
                                        {
                                            panel.x + 16, panel.y + 56,
                                            panel.width - 32, panel.height - 72
                                        })) {
            const float contentHeight =
                    static_cast<float>(
                        std::count(m_ticketPreview.begin(), m_ticketPreview.end(), '\n') +
                        1) *
                    24.f;
            const float maxScroll =
                    std::max(0.f, contentHeight - (panel.height - 88.f));
            m_ticketPreviewScroll = std::clamp(
                m_ticketPreviewScroll - GetMouseWheelMove() * 24.f, 0.f, maxScroll);
        }
        return;
    }
    layout();
    const float rowUnit = std::max(1.f, std::min(m_area.width, m_area.height));
    if (std::abs(rowUnit - m_lastRowUnit) > 1.f) {
        m_lastRowUnit = rowUnit;
        rebuildRows();
    }
    m_printButton->update(deltaSeconds);
    m_drawerButton->update(deltaSeconds);
    for (const auto& button: m_numericButtons)
        button->update(deltaSeconds);
    m_addManualButton->update(deltaSeconds);
    m_items->update(deltaSeconds);
}

void SelectedProductsScreen::draw() {
    layout();
    m_title->draw();
    m_items->draw();
    const float inset =
            std::max(1.f, std::min(m_area.width, m_area.height) * .035f);
    const float totalHeight = m_area.height * .095f;
    const float bottom = m_area.y + m_area.height - inset;
    const float totalY = bottom - totalHeight;
    const float keypadY = totalY - m_area.height * .34f;
    const float amountHeight = m_area.height * .34f * .18f;
    DrawRectangleRounded(
        {m_area.x + inset, keypadY, m_area.width - 2 * inset, amountHeight}, .12f,
        8, Color{42, 51, 66, 255});
    const float totalBlockX = m_area.x + inset;
    const float totalBlockWidth = (m_area.width - 2.f * inset) * .46f;
    DrawRectangleRounded({totalBlockX, totalY, totalBlockWidth, totalHeight},
                         .16f, 8, Color{42, 51, 66, 255});
    m_totalLabel->draw();
    m_manualAmountLabel->draw();
    for (const auto& button: m_numericButtons)
        button->draw();
    m_addManualButton->draw();
    m_printButton->draw();
    m_drawerButton->draw();
    if (m_ticketPreviewOpen) {
        const float w = std::min(640.f, m_area.width * .88f);
        const float h = std::min(620.f, m_area.height * .88f);
        const Rectangle panel{
            m_area.x + (m_area.width - w) * .5f,
            m_area.y + (m_area.height - h) * .5f, w, h
        };
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                      Color{0, 0, 0, 175});
        DrawRectangleRounded(panel, .025f, 8, Color{27, 34, 45, 255});
        NexoBarImp app;
        DrawTextEx(
            *m_font,
            app->text("lists.preview_title", "Vista previa del ticket").c_str(),
            {panel.x + 18, panel.y + 16}, 22, 1, RAYWHITE);
        const Rectangle close{
            panel.x + panel.width - 48.f, panel.y + 10.f, 36.f,
            36.f
        };
        DrawRectangleRounded(close, .2f, 6, Color{75, 48, 52, 255});
        DrawTextEx(*m_font, "X", {close.x + 11, close.y + 7}, 20, 1, RAYWHITE);
        const Rectangle viewport{
            panel.x + 16, panel.y + 56, panel.width - 32,
            panel.height - 72
        };
        BeginScissorMode(static_cast<int>(viewport.x), static_cast<int>(viewport.y),
                         static_cast<int>(viewport.width),
                         static_cast<int>(viewport.height));
        float y = viewport.y + 8.f - m_ticketPreviewScroll;
        std::istringstream stream(m_ticketPreview);
        std::string line;
        while (std::getline(stream, line)) {
            DrawTextEx(*m_font, line.c_str(), {viewport.x + 8, y}, 18, 1,
                       Color{225, 230, 238, 255});
            y += 24.f;
        }
        EndScissorMode();
    }
}
