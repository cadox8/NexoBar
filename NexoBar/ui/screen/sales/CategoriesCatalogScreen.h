//
// Created by cadox8 on 24/10/2026.
//
#pragma once

#include "ProductState.h"
#include "../../components/layout/ScrollableBlock.h"
#include "../../components/product/ProductCard.h"
#include "../../components/button/Button.h"
#include "../../components/text/StaticText.h"
#include "../IScreen.h"

#include <memory>
#include <optional>
#include <cstddef>
#include <vector>

class CategoriesCatalogScreen final : public IScreen {
public:
    explicit CategoriesCatalogScreen(std::shared_ptr<ProductState> state);

    boost::signals2::signal<void(std::int64_t)> categorySelected;
    boost::signals2::signal<void()> addRequested;
    boost::signals2::signal<void(std::int64_t)> deleteRequested;
    boost::signals2::signal<void(std::int64_t)> editRequested;

    void refresh();

    void update(float deltaSeconds) override;

    void draw() override;

private:
    void layout();

    std::shared_ptr<ProductState> m_state;
    ScrollableBlockPtr m_catalog;
    StaticTextPtr m_title;
    StaticTextPtr m_emptyLabel;
    std::vector<ProductCardPtr> m_categoryCards;
    ButtonPtr m_addButton, m_editButton, m_deleteButton;
    std::optional<std::int64_t> m_pendingCategory;
    std::optional<std::int64_t> m_pendingEdit, m_pendingDelete;

    enum class ActionMode { Normal, Edit, Delete };

    ActionMode m_actionMode = ActionMode::Normal;

    void setActionMode(ActionMode mode);

    FontPtr m_font;
};
