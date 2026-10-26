#include "ProductScreen.h"

#include "../menus/HeaderScreen.h"
#include "../menus/SideMenuScreen.h"
#include "../settings/SettingsHomeScreen.h"
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

    auto page = std::make_shared<ScreenLayout>(LayoutAxis::Vertical);
    page->addWeighted("sale", sale);

    auto pageHost = std::make_shared<ScreenLayout>(LayoutAxis::Vertical);
    pageHost->addWeighted("page", page);
    const auto settings = std::make_shared<SettingsHomeScreen>(
        [state](const std::string& key) { return state->hasPermission(key); });
    const auto showSale = [pageHost, page] { pageHost->setScreen("page", page); };
    const auto showSettings = [pageHost, settings] {
        pageHost->setScreen("page", settings);
    };

    auto body = std::make_shared<ScreenLayout>(LayoutAxis::Horizontal);
    body->gap(8.f);
    LayoutOptions menuOptions;
    menuOptions.width = LayoutSize::px(44.f);
    body->addScreen("menu", std::make_shared<SideMenuScreen>(showSale, showSettings, state->canOpenSettings()), menuOptions);
    body->addWeighted("content", pageHost, 1.f);

    LayoutOptions headerOptions;
    headerOptions.height = LayoutSize::px(44.f);
    addScreen("header", std::make_shared<HeaderScreen>(std::move(state)), headerOptions);
    addWeighted("body", body, 9.f);
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
