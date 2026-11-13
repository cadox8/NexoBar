#include "CatalogPopupScreen.h"
#include "../../../NexoBar.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

CatalogPopupScreen::CatalogPopupScreen(std::shared_ptr<ProductState> state) : m_state(std::move(state)),
                                                                              m_name(std::make_shared<InputText>()), m_icon(std::make_shared<InputText>()), m_price(std::make_shared<InputText>()),
                                                                              m_category(std::make_shared<DropdownComponent>()), m_tax(std::make_shared<DropdownComponent>()),
                                                                              m_save(std::make_shared<Button>()), m_cancel(std::make_shared<Button>()) {
    NexoBarImp nexo;
    m_font = nexo->defaultFont();
    m_save->text = nexo->text("categories.save");
    m_cancel->text = nexo->text("categories.cancel");
    for (auto f: {m_name, m_icon, m_price}) {
        f->font = m_font;
        f->fontSize = 17;
        f->background = Color{34, 42, 55, 255};
    }
    m_name->placeholder = nexo->text("categories.name");
    m_icon->placeholder = nexo->text("categories.icon_placeholder");
    m_price->placeholder = nexo->text("categories.price_placeholder");
    for (auto d: {m_category, m_tax}) {
        d->font = m_font;
        d->fontSize = 16;
        d->optionHeight = 38;
    }
    for (auto b: {m_save, m_cancel}) {
        b->font = m_font;
        b->fontSize = 16;
        b->roundness = .18f;
        b->padding = LayoutInsets::symmetric(12, 6);
    }
    m_save->background = Color{35, 130, 100, 255};
    m_save->hoverBackground = Color{43, 155, 119, 255};
    m_cancel->background = Color{51, 62, 79, 255};
    m_cancel->hoverBackground = Color{67, 81, 101, 255};
    m_save->onClick([this] { save(); });
    m_cancel->onClick([this] {
        m_visible = false;
        if (onClose)onClose();
    });
    configureOptions();
}

void CatalogPopupScreen::configureOptions() {
    NexoBarImp nexo;
    std::vector<DropdownOption> cats, taxes;
    for (const auto& c: m_state->categories) {
        const auto v = std::to_string(c.id);
        cats.push_back({c.name, v});
    }
    for (const auto& t: m_state->taxes) {
        std::string rate = std::to_string(t.rate / 100);
        if (t.rate % 100) {
            auto cents = std::to_string(std::abs(t.rate % 100));
            if (cents.size() < 2)cents = "0" + cents;
            rate += "." + cents;
        }
        taxes.push_back({t.name + " (" + rate + "%)", std::to_string(t.id)});
    }
    m_category->setOptions(std::move(cats), false);
    m_tax->setOptions(std::move(taxes), false);
}

void CatalogPopupScreen::showCategory() {
    m_productForm = false;
    m_editId = 0;
    m_visible = true;
    m_error.clear();
    m_name->setText("");
    m_icon->setText("logo.png");
    configureOptions();
}

void CatalogPopupScreen::showProduct(std::int64_t categoryId) {
    m_productForm = true;
    m_editId = 0;
    m_fixedCategoryId = categoryId > 0 ? std::optional<std::int64_t>{categoryId} : std::nullopt;
    m_visible = true;
    m_error.clear();
    m_name->setText("");
    m_price->setText("");
    configureOptions();
    m_category->enabled = !m_fixedCategoryId.has_value();
    if (m_fixedCategoryId)for (std::size_t i = 0; i < m_category->options.size(); ++i)if (m_category->options[i].value == std::to_string(categoryId))m_category->select(static_cast<int>(i));
}

void CatalogPopupScreen::editCategory(std::int64_t id) {
    showCategory();
    const auto it = std::ranges::find(m_state->categories, id, &ProductState::Category::id);
    if (it == m_state->categories.end())return;
    m_editId = id;
    m_name->setText(it->name);
    m_icon->setText(it->icon);
}

void CatalogPopupScreen::editProduct(std::int64_t id) {
    showProduct(0);
    m_category->enabled = true;
    const auto it = std::ranges::find(m_state->products, id, &ProductState::Product::id);
    if (it == m_state->products.end())return;
    m_editId = id;
    m_name->setText(it->name);
    m_price->setText(std::to_string(it->priceMinor / 100) + "." + (it->priceMinor % 100 < 10 ? "0" : "") + std::to_string(it->priceMinor % 100));
    for (std::size_t i = 0; i < m_category->options.size(); ++i)if (m_category->options[i].value == std::to_string(it->categoryId))m_category->select(static_cast<int>(i));
    for (std::size_t i = 0; i < m_tax->options.size(); ++i)if (m_tax->options[i].value == std::to_string(it->taxId))m_tax->select(static_cast<int>(i));
}

void CatalogPopupScreen::layout() {
    const float w = std::min(560.f, m_area.width * .88f), h = std::min(m_productForm ? 405.f : 270.f, m_area.height * .84f);
    m_panel = {m_area.x + (m_area.width - w) * .5f, m_area.y + (m_area.height - h) * .5f, w, h};
    const float pad = std::clamp(w * .045f, 16.f, 26.f), row = 44.f, gap = 12.f;
    float y = m_panel.y + 62.f;
    m_name->area({m_panel.x + pad, y, w - 2 * pad, row});
    y += row + gap;
    if (m_productForm) {
        m_category->area({m_panel.x + pad, y, w - 2 * pad, row});
        y += row + gap;
        m_tax->area({m_panel.x + pad, y, w - 2 * pad, row});
        y += row + gap;
        m_price->area({m_panel.x + pad, y, w - 2 * pad, row});
        y += row + gap;
    } else {
        m_icon->area({m_panel.x + pad, y, w - 2 * pad, row});
    }
    m_cancel->area({m_panel.x + w - pad - 204, m_panel.y + h - 52, 96, 36});
    m_save->area({m_panel.x + w - pad - 96, m_panel.y + h - 52, 96, 36});
}

void CatalogPopupScreen::save() {
    NexoBarImp nexo;
    const auto name = m_name->text;
    if (name.empty()) {
        m_error = nexo->text("categories.name_required");
        return;
    }
    try {
        if (!m_productForm) {
            const bool saved = m_editId ? m_state->updateCategory(m_editId, name, m_icon->text) : m_state->createCategory(name, m_icon->text);
            if (!saved) {
                m_error = nexo->text("categories.category_save_failed");
                return;
            }
        } else {
            const auto* c = m_category->selected();
            const auto* t = m_tax->selected();
            if (!c || !t) {
                m_error = nexo->text("categories.select_category_tax");
                return;
            }
            std::string value = m_price->text;
            std::replace(value.begin(), value.end(), ',', '.');
            char* end = nullptr;
            const double amount = std::strtod(value.c_str(), &end);
            if (value.empty() || end != value.c_str() + value.size() || !std::isfinite(amount) || amount < 0 || amount > 90000000000.0) {
                m_error = nexo->text("categories.invalid_price");
                return;
            }
            const bool saved = m_editId ? m_state->updateProduct(m_editId, name, std::stoll(c->value), std::stoll(t->value), static_cast<std::int64_t>(std::llround(amount * 100))) : m_state->createProduct(name, std::stoll(c->value), std::stoll(t->value), static_cast<std::int64_t>(std::llround(amount * 100)));
            if (!saved) {
                m_error = nexo->text("categories.product_save_failed");
                return;
            }
        }
        m_visible = false;
        m_error.clear();
        if (onSaved)onSaved();
    } catch (const std::exception& error) {
        m_error = nexo->userError(error);
    }
}

void CatalogPopupScreen::update(float dt) {
    if (!m_visible)return;
    layout();
    m_name->update(dt);
    if (m_productForm) {
        m_category->update(dt);
        m_tax->update(dt);
        m_price->update(dt);
    } else {
        m_icon->update(dt);
    }
    m_save->update(dt);
    m_cancel->update(dt);
}

void CatalogPopupScreen::draw() {
    if (!m_visible)return;
    layout();
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{0, 0, 0, 155});
    DrawRectangleRounded(m_panel, .08f, 10, Color{27, 34, 46, 255});
    DrawRectangleRoundedLinesEx(m_panel, .08f, 10, 1.5f, Color{74, 91, 112, 255});
    NexoBarImp nexo;
    const std::string title = m_productForm
        ? nexo->text(m_editId ? "categories.edit_product" : "categories.add_product")
        : nexo->text(m_editId ? "categories.edit_category" : "categories.add_category");
    DrawTextEx(*m_font, title.c_str(), {m_panel.x + 22, m_panel.y + 18}, 22, 1, RAYWHITE);
    m_name->draw();
    if (m_productForm) {
        m_category->draw();
        m_tax->draw();
        m_price->draw();
    } else {
        m_icon->draw();
    }
    m_cancel->draw();
    m_save->draw();
    if (!m_error.empty())DrawTextEx(*m_font, m_error.c_str(), {m_panel.x + 22, m_panel.y + m_panel.height - 82}, 15, 1, Color{255, 135, 125, 255});
    // Dropdown popups are rendered last so they remain above every field and message.
    if (m_productForm) {
        if (m_category->open)m_category->drawOverlay();
        if (m_tax->open)m_tax->drawOverlay();
    }
}
