#include "ProductScreen.h"

#include "CategoriesCatalogScreen.h"
#include "ProductCatalogScreen.h"
#include "SelectedProductsScreen.h"

#include "../../components/layout/Layout.h"
#include <utility>

ProductScreen::ProductScreen(std::shared_ptr<ProductState> state) {
    auto categories = std::make_shared<CategoriesCatalogScreen>(state);
    auto products = std::make_shared<ProductCatalogScreen>(state);
    auto order = std::make_shared<SelectedProductsScreen>(
        state, products->productSelected);
    m_popup = std::make_shared<CatalogPopupScreen>(state);
    auto catalogHost = std::make_shared<ScreenLayout>(LayoutAxis::Vertical);
    catalogHost->addScreen("catalog", categories);
    m_categoryConnection = categories->categorySelected.connect(
        [products, catalogHost](const std::int64_t categoryId) {
            products->setCategory(categoryId);
            catalogHost->setScreen("catalog", products);
        });
    m_backConnection = products->backRequested.connect([catalogHost, categories] {
        catalogHost->setScreen("catalog", categories);
    });
    std::weak_ptr<CatalogPopupScreen> popup = m_popup;
    m_addCategoryConnection = categories->addRequested.connect([popup] {
        if (auto p = popup.lock())
            p->showCategory();
    });
    m_deleteCategoryConnection = categories->deleteRequested.connect(
        [state, categories, products](std::int64_t id) {
            if (state->deactivateCategory(id)) {
                categories->refresh();
                products->refresh();
            }
        });
    m_addProductConnection = products->addRequested.connect([popup, products] {
        if (auto p = popup.lock())
            p->showProduct(products->categoryId());
    });
    m_deleteProductConnection = products->deleteRequested.connect(
        [state, categories, products](std::int64_t id) {
            if (state->deactivateProduct(id)) {
                categories->refresh();
                products->refresh();
            }
        });
    m_editCategoryConnection =
            categories->editRequested.connect([popup](std::int64_t id) {
                if (auto p = popup.lock())
                    p->editCategory(id);
            });
    m_editProductConnection =
            products->editRequested.connect([popup](std::int64_t id) {
                if (auto p = popup.lock())
                    p->editProduct(id);
            });
    m_popup->onSaved = [categories, products] {
        categories->refresh();
        products->refresh();
    };

    if (!state->categories.empty())
        products->setCategory(state->categories.front().id);

    auto sale = std::make_shared<ScreenLayout>(LayoutAxis::Horizontal);
    sale->gap(8.f);
    sale->addWeighted("catalog", catalogHost, 6.4f);
    sale->addWeighted("order", order, 3.6f);

    addWeighted("sale", sale);
}

void ProductScreen::update(float dt) {
    if (m_popup && m_popup->visible()) {
        m_popup->area(m_area);
        m_popup->update(dt);
        return;
    }
    ScreenLayout::update(dt);
}

void ProductScreen::draw() {
    ScreenLayout::draw();
    if (m_popup) {
        m_popup->area(m_area);
        m_popup->draw();
    }
}
