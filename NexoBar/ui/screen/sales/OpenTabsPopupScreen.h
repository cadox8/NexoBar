#pragma once

#include "ProductState.h"
#include "../IScreen.h"
#include "../../components/button/Button.h"
#include "../../components/text/InputText.h"
#include "../../components/text/VirtualKeyboard.h"
#include "database/models/ConsumptionList.h"
#include "database/models/ListLine.h"
#include "sales/SaleService.h"

#include <memory>
#include <set>
#include <vector>

class OpenTabsPopupScreen final : public IScreen {
public:
    explicit OpenTabsPopupScreen(std::shared_ptr<ProductState> state);

    void showAccounts();
    void showAddCurrent();
    [[nodiscard]] bool visible() const noexcept { return m_visible; }

    void update(float deltaSeconds) override;
    void draw() override;

private:
    enum class Mode { Browse, Add, Create };
    enum class ConfirmAction { None, RemoveLines, CancelAccount };

    void layout();
    void reload();
    void select(nexobar::ConsumptionListId id);
    void showCreate();
    void createAccount();
    void addCurrentTo(nexobar::ConsumptionListId id);
    void printSelected();
    void removeSelectedLines();
    void cancelSelectedAccount();
    void confirmAction();
    void close();
    [[nodiscard]] std::vector<nexobar::SaleItem> selectedSaleItems() const;

    std::shared_ptr<ProductState> m_state;
    FontPtr m_font;
    ButtonPtr m_close, m_newAccount, m_action, m_cancelCreate;
    ButtonPtr m_removeLines, m_cancelAccount, m_confirmButton, m_cancelConfirm;
    InputTextPtr m_name;
    VirtualKeyboardPtr m_keyboard;
    std::vector<ButtonPtr> m_accountButtons;
    std::vector<nexobar::ConsumptionList> m_accounts;
    std::vector<nexobar::ListLine> m_lines;
    std::optional<nexobar::ConsumptionListId> m_selected;
    std::set<nexobar::ListLineId> m_selectedLines;
    Mode m_mode = Mode::Browse;
    Mode m_returnMode = Mode::Browse;
    ConfirmAction m_confirm = ConfirmAction::None;
    bool m_visible = false;
    std::string m_error;
    Rectangle m_panel{}, m_left{}, m_right{};
    float m_detailScroll = 0.f;
};
