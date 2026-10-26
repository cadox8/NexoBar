#pragma once

#include "ProductState.h"
#include "../../components/layout/ScrollableBlock.h"
#include "../../components/product/ProductCard.h"
#include "../../components/button/Button.h"
#include "../../components/text/StaticText.h"
#include "../IScreen.h"

#include <boost/signals2.hpp>

#include <memory>
#include <optional>
#include <vector>

class ProductCatalogScreen final : public IScreen {
public:
    explicit ProductCatalogScreen(std::shared_ptr<ProductState> state);

    void setCategory(std::int64_t categoryId);

    std::int64_t categoryId() const noexcept { return m_selectedCategory.value_or(0); }

    boost::signals2::signal<void(std::size_t)> productSelected;
    boost::signals2::signal<void()> backRequested, addRequested;
    boost::signals2::signal<void(std::int64_t)> deleteRequested;
    boost::signals2::signal<void(std::int64_t)> editRequested;

    void refresh();

    void update(float deltaSeconds) override;

    void draw() override;

private:
    void rebuildProductGrid();

    void layout();

    std::shared_ptr<ProductState> m_state;
    ScrollableBlockPtr m_catalog;
    StaticTextPtr m_title;
    StaticTextPtr m_emptyLabel;
    ButtonPtr m_backButton, m_addButton, m_editButton, m_deleteButton;
    std::vector<ProductCardPtr> m_productCards;
    std::optional<std::int64_t> m_selectedCategory;
    std::optional<std::int64_t> m_pendingEdit, m_pendingDelete;

    enum class ActionMode { Normal, Edit, Delete };

    ActionMode m_actionMode = ActionMode::Normal;

    void setActionMode(ActionMode mode);

    FontPtr m_font;
    int m_lastWidth = 0;
    int m_lastHeight = 0;
};
