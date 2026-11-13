#pragma once
#include "ProductState.h"
#include "../IScreen.h"
#include "../../components/text/InputText.h"
#include "../../components/controls/DropdownComponent.h"
#include "../../components/button/Button.h"
#include <functional>
#include <optional>

class CatalogPopupScreen final : public IScreen {
public:
    explicit CatalogPopupScreen(std::shared_ptr<ProductState> state);

    void showCategory();

    void showProduct(std::int64_t categoryId);

    void editCategory(std::int64_t id);

    void editProduct(std::int64_t id);

    bool visible() const noexcept { return m_visible; }
    std::function<void()> onClose, onSaved;

    void update(float deltaSeconds) override;

    void draw() override;

private:
    void layout();

    void save();

    void configureOptions();

    std::shared_ptr<ProductState> m_state;
    InputTextPtr m_name, m_icon, m_price;
    DropdownComponentPtr m_category, m_tax;
    ButtonPtr m_save, m_cancel;
    FontPtr m_font;
    bool m_visible = false, m_productForm = false;
    std::int64_t m_editId = 0;
    std::optional<std::int64_t> m_fixedCategoryId;
    std::string m_error;
    Rectangle m_panel{};
};
