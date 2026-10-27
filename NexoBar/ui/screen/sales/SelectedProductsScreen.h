#pragma once

#include "../../components/button/Button.h"
#include "../../components/layout/ScrollableBlock.h"
#include "../../components/text/StaticText.h"
#include "../IScreen.h"
#include "ProductState.h"
#include "sales/SaleService.h"

#include <boost/signals2.hpp>

#include <memory>
#include <string>
#include <vector>

class SelectedProductsScreen final : public IScreen {
public:
    SelectedProductsScreen(
        std::shared_ptr<ProductState> state,
        boost::signals2::signal<void(std::size_t)>& productSelected);

    void update(float deltaSeconds) override;

    void draw() override;

private:
    void layout();

    void appendManualAmount();

    void appendManualKey(const std::string& key);

    void updateManualAmountDisplay();

    void printTicket();

    void rebuildRows();

    std::shared_ptr<ProductState> m_state;
    boost::signals2::connection m_productConnection;
    boost::signals2::connection m_orderConnection;
    ScrollableBlockPtr m_items;
    StaticTextPtr m_title;
    StaticTextPtr m_totalLabel;
    StaticTextPtr m_manualAmountLabel;
    ButtonPtr m_printButton;
    ButtonPtr m_drawerButton;
    ButtonPtr m_addManualButton;
    std::vector<ButtonPtr> m_numericButtons;
    std::string m_manualAmountInput;
    std::string m_decimalSeparator = ",";
    FontPtr m_font;
    float m_lastRowUnit = 0.f;
    std::string m_ticketPreview;
    float m_ticketPreviewScroll = 0.f;
    bool m_ticketPreviewOpen = false;
};
