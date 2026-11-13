#include "SelectedProductsScreen.h"

#include "../../../NexoBar.h"
#include "printer/TicketPrinter.h"
#include "database/hospitality/HospitalityDatabase.h"

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <memory>
#include <utility>

#include "log/Log.h"

namespace {
    constexpr Color Accent{.r = 91, .g = 207, .b = 169, .a = 255};

} // namespace

SelectedProductsScreen::SelectedProductsScreen(
    std::shared_ptr<ProductState> state,
    boost::signals2::signal<void(std::size_t)>& productSelected)
    : m_state(std::move(state)),
      m_items(std::make_shared<ScrollableBlock>(LayoutAxis::Vertical, Panel)),
      m_title(std::make_shared<StaticText>()),
      m_totalLabel(std::make_shared<StaticText>()),
      m_manualAmountLabel(std::make_shared<StaticText>()),
      m_accountsButton(std::make_shared<Button>()),
      m_addToAccountButton(std::make_shared<Button>()),
      m_drawerButton(std::make_shared<Button>()),
      m_addManualButton(std::make_shared<Button>()) {

    NexoBarImp app;

    m_font = app->defaultFont();
    m_decimalSeparator = app->text("number.decimal_separator");
    m_title->text = app->text("lists.title");
    m_title->font = m_font;
    m_title->fontSize = 30;
    m_title->color = RAYWHITE;
    m_title->horizontalAlignment = TextHorizontalAlignment::Center;
    m_totalLabel->font = m_font;
    m_totalLabel->fontSize = 24;
    m_totalLabel->color = Accent;
    m_totalLabel->horizontalAlignment = TextHorizontalAlignment::Center;
    m_totalLabel->verticalAlignment = TextVerticalAlignment::Center;
    m_manualAmountLabel->font = m_font;
    m_manualAmountLabel->fontSize = 20;
    m_manualAmountLabel->color = RAYWHITE;
    m_manualAmountLabel->horizontalAlignment = TextHorizontalAlignment::Center;
    updateManualAmountDisplay();

    m_accountsButton->font = m_font;
    m_accountsButton->fontSize = 16;
    m_accountsButton->text = app->text("open_tabs.view_accounts");
    m_accountsButton->background = Color{52, 105, 170, 255};
    m_accountsButton->textColor = RAYWHITE;
    m_accountsButton->onClick([this] { viewAccountsRequested(); });

    m_addToAccountButton->font = m_font;
    m_addToAccountButton->fontSize = 16;
    m_addToAccountButton->text = app->text("open_tabs.send_ticket");
    m_addToAccountButton->background = Color{35, 130, 100, 255};
    m_addToAccountButton->textColor = RAYWHITE;
    m_addToAccountButton->onClick([this] { addToAccountRequested(); });

    m_drawerButton->font = m_font;
    m_drawerButton->fontSize = 16;
    m_drawerButton->text = app->text("lists.open_drawer");
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
    m_drawerButton->enabled = m_state->hasPermission("open_drawer");

    m_addManualButton->font = m_font;
    m_addManualButton->fontSize = 18;
    m_addManualButton->text = app->text("lists.add_manual");
    m_addManualButton->background = Color{.r = 255, .g = 218, .b = 142, .a = 255};
    m_addManualButton->hoverBackground =
            Color{.r = 255, .g = 228, .b = 162, .a = 255};
    m_addManualButton->pressedBackground =
            Color{.r = 238, .g = 198, .b = 116, .a = 255};
    m_addManualButton->textColor = BLACK;
    m_addManualButton->onClick([&] { appendManualAmount(); });

    const std::string backspace = app->text("lists.backspace");
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
    m_accountsButton->fontSize = m_addManualButton->fontSize;
    m_addToAccountButton->fontSize = m_addManualButton->fontSize;
    m_drawerButton->fontSize = m_addManualButton->fontSize;
    for (const auto& button: m_numericButtons)
        button->fontSize =
                static_cast<int>(std::clamp(20.f * fontScale, 13.f, 27.f));
    m_title->area(
        {listX, m_area.y + inset * .4f, listWidth * .28f, headingHeight - inset * .4f});
    const float headingButtonY = m_area.y + inset * .55f;
    const float headingButtonH = headingHeight - inset * .8f;
    m_accountsButton->area({listX + listWidth * .30f, headingButtonY,
                            listWidth * .32f, headingButtonH});
    m_addToAccountButton->area({listX + listWidth * .64f, headingButtonY,
                                listWidth * .36f, headingButtonH});
    m_addToAccountButton->enabled = !m_state->selectedOrder.empty();
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

    m_totalLabel->text = app->text("receipt.total") +
                         ": " + m_state->formatPrice(m_state->total());
    const Rectangle totalArea{
        .x = listX,
        .y = totalY + totalHeight * .12f,
        .width = listWidth * .70f,
        .height = totalHeight * .76f
    };
    const float preferredTotalSize = std::clamp(25.f * fontScale, 20.f, 36.f);
    const Vector2 measuredTotal = MeasureTextEx(*m_font, m_totalLabel->text.c_str(),
                                                preferredTotalSize, 1.f);
    const float widthScale = (totalArea.width - 12.f) /
                             std::max(1.f, measuredTotal.x);
    const float heightScale = (totalArea.height - 8.f) /
                              std::max(1.f, measuredTotal.y);
    m_totalLabel->fontSize = std::max(
        6.f, preferredTotalSize * std::min({1.f, widthScale, heightScale}));
    m_totalLabel->area(totalArea);
    m_drawerButton->area({
        .x = listX + listWidth * .72f,
        .y = totalY + totalHeight * .12f,
        .width = listWidth * .28f,
        .height = totalHeight * .76f
    });
}

void SelectedProductsScreen::updateManualAmountDisplay() {
    NexoBarImp app;
    const std::string amount = m_manualAmountInput.empty()
                                   ? "0" + m_decimalSeparator + "00"
                                   : m_manualAmountInput;
    m_manualAmountLabel->text = app->text("lists.amount_prefix") + amount;
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
    const std::string name = app->text("lists.manual_amount");
    std::int64_t manualId = -1;
    for (const auto& product: m_state->products)
        if (product.id <= manualId)
            manualId = product.id - 1;
    const std::int64_t taxId =
            m_state->taxes.empty() ? 0 : m_state->taxes.front().id;
    m_state->products.push_back({manualId, -1, name, price, taxId});
    m_state->quantities.push_back(0);
    m_state->addProduct(m_state->products.size() - 1);
    m_manualAmountInput.clear();
    updateManualAmountDisplay();
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
        const float fontSize = std::clamp(unit * .052f, 16.f, 23.f);
        const float gap = std::max(2.f, unit * .012f);
        auto row = std::make_shared<BlockComponent>(LayoutAxis::Horizontal,
                                                    Color{36, 44, 57, 255});
        row->roundness = .10f;
        row->content.gap(gap).padding(LayoutInsets::all(std::max(6.f, gap)));
        std::string quantityFormat = app->text("lists.quantity_format");
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
        auto removeButton = std::make_shared<Button>(app->text("common.remove_symbol"));
        removeButton->font = m_font;
        removeButton->fontSize = static_cast<int>(std::clamp(fontSize - 4.f, 12.f, 16.f));
        removeButton->padding = LayoutInsets::all(std::max(3.f, gap * .45f));
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
        row->add(removeButton, LayoutSize::px(std::clamp(unit * .085f, 28.f, 35.f)),
                 LayoutSize::fill());
        m_items->content.add(
            row, LayoutSize::fill(),
            LayoutSize::px(std::clamp(unit * .14f, 50.f, 68.f)),
            LayoutInsets::symmetric(std::max(6.f, unit * .015f), 4.f));
    }
}

void SelectedProductsScreen::update(const float deltaSeconds) {
    layout();
    const float rowUnit = std::max(1.f, std::min(m_area.width, m_area.height));
    if (std::abs(rowUnit - m_lastRowUnit) > 1.f) {
        m_lastRowUnit = rowUnit;
        rebuildRows();
    }
    m_accountsButton->update(deltaSeconds);
    m_addToAccountButton->update(deltaSeconds);
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
    const float totalBlockWidth = (m_area.width - 2.f * inset) * .70f;
    DrawRectangleRounded({totalBlockX, totalY + totalHeight * .12f,
                          totalBlockWidth, totalHeight * .76f},
                         .16f, 8, Color{42, 51, 66, 255});
    m_totalLabel->draw();
    m_manualAmountLabel->draw();
    for (const auto& button: m_numericButtons)
        button->draw();
    m_addManualButton->draw();
    m_accountsButton->draw();
    m_addToAccountButton->draw();
    m_drawerButton->draw();
}
