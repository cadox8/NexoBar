#pragma once

#include "../ScreenLayout.h"
#include "ProductState.h"
#include "CatalogPopupScreen.h"
#include "OpenTabsPopupScreen.h"

#include <boost/signals2.hpp>
#include <memory>

class ProductScreen final : public ScreenLayout {
public:
    explicit ProductScreen(std::shared_ptr<ProductState> state);

private:
    boost::signals2::connection m_categoryConnection;
    boost::signals2::connection m_backConnection;
    boost::signals2::connection m_addCategoryConnection, m_deleteCategoryConnection;
    boost::signals2::connection m_addProductConnection, m_deleteProductConnection;
    boost::signals2::connection m_editCategoryConnection, m_editProductConnection;
    boost::signals2::connection m_viewAccountsConnection, m_addToAccountConnection;
    std::shared_ptr<CatalogPopupScreen> m_popup;
    std::shared_ptr<OpenTabsPopupScreen> m_accountsPopup;

public:
    void update(float deltaSeconds) override;

    void draw() override;
};
