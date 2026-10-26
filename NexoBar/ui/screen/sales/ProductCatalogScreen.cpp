#include "ProductCatalogScreen.h"

#include "../../../NexoBar.h"

#include <algorithm>
#include <cmath>
#include <utility>

ProductCatalogScreen::ProductCatalogScreen(std::shared_ptr<ProductState> state)
    : m_state(std::move(state)),
      m_catalog(std::make_shared<ScrollableBlock>(LayoutAxis::Vertical, Panel)),
      m_title(std::make_shared<StaticText>()),
      m_emptyLabel(std::make_shared<StaticText>()) {
    NexoBarImp app;
    m_font = app->defaultFont();
    m_title->font = m_font;
    m_title->color = RAYWHITE;
    m_emptyLabel->font = m_font;
    m_emptyLabel->color = Color{190, 199, 214, 255};
    m_emptyLabel->horizontalAlignment = TextHorizontalAlignment::Center;
    m_backButton = std::make_shared<Button>("Volver");
    m_addButton = std::make_shared<Button>("+");
    m_editButton = std::make_shared<Button>("Editar");
    m_deleteButton = std::make_shared<Button>("Eliminar");
    for (auto b: {m_backButton, m_addButton, m_editButton, m_deleteButton}) {
        b->font = m_font;
        b->fontSize = 15;
        b->roundness = .18f;
        b->background = Color{39, 50, 66, 255};
        b->hoverBackground = Color{57, 73, 94, 255};
        b->textColor = RAYWHITE;
        b->padding = LayoutInsets::symmetric(7, 4);
    }
    m_backButton->fontSize = 16;
    m_backButton->padding = LayoutInsets::symmetric(6, 3);
    m_backButton->onClick([this] { backRequested(); });
    m_addButton->onClick([this] { addRequested(); });
    m_editButton->image = NexoBarImp::get()->assets().getTexture("icon-edit");
    m_deleteButton->image = NexoBarImp::get()->assets().getTexture("icon-delete");
    m_editButton->text.clear();
    m_deleteButton->text.clear();
    m_editButton->onClick([this] { setActionMode(m_actionMode == ActionMode::Edit ? ActionMode::Normal : ActionMode::Edit); });
    m_deleteButton->onClick([this] { setActionMode(m_actionMode == ActionMode::Delete ? ActionMode::Normal : ActionMode::Delete); });
    refresh();
}

void ProductCatalogScreen::setActionMode(ActionMode mode) {
    m_actionMode = mode;
    m_editButton->background = mode == ActionMode::Edit ? Color{224, 171, 53, 255} : Color{39, 50, 66, 255};
    m_editButton->hoverBackground = mode == ActionMode::Edit ? Color{240, 190, 71, 255} : Color{57, 73, 94, 255};
    m_deleteButton->background = mode == ActionMode::Delete ? Color{190, 63, 70, 255} : Color{39, 50, 66, 255};
    m_deleteButton->hoverBackground = mode == ActionMode::Delete ? Color{212, 77, 83, 255} : Color{57, 73, 94, 255};
}

void ProductCatalogScreen::refresh() {
    m_productCards.clear();
    m_productCards.reserve(m_state->products.size());
    for (std::size_t index = 0; index < m_state->products.size(); ++index) {
        const auto& product = m_state->products[index];
        auto card = std::make_shared<ProductCard>(product.name, m_state->formatPrice(product.priceMinor),
                                                  [this, index] {
                                                      const auto id = m_state->products[index].id;
                                                      if (m_actionMode == ActionMode::Edit) {
                                                          m_pendingEdit = id;
                                                          setActionMode(ActionMode::Normal);
                                                          return;
                                                      }
                                                      if (m_actionMode == ActionMode::Delete) {
                                                          m_pendingDelete = id;
                                                          setActionMode(ActionMode::Normal);
                                                          return;
                                                      }
                                                      productSelected(index);
                                                  });
        card->background = Color{36, 44, 57, 255};
        card->hoverBackground = Color{48, 61, 78, 255};
        card->priceColor = Color{91, 207, 169, 255};
        card->font = m_font;
        m_productCards.push_back(std::move(card));
    }
    if (m_selectedCategory) {
        const auto found = std::ranges::find_if(m_state->categories, [this](const auto& c) { return c.id == *m_selectedCategory; });
        if (found == m_state->categories.end()) m_selectedCategory.reset();
    }
    const auto category = std::ranges::find_if(m_state->categories, [this](const auto& c) { return m_selectedCategory && c.id == *m_selectedCategory; });
    m_title->text = category == m_state->categories.end() ? std::string{} : category->name;
    rebuildProductGrid();
}

void ProductCatalogScreen::setCategory(const std::int64_t categoryId) {
    if (m_selectedCategory == categoryId) return;
    m_selectedCategory = categoryId;
    const auto found = std::ranges::find_if(m_state->categories,
                                            [categoryId](const auto& value) { return value.id == categoryId; });
    m_title->text = found == m_state->categories.end() ? std::string{} : found->name;
    rebuildProductGrid();
}

void ProductCatalogScreen::rebuildProductGrid() {
    const float inset = std::max(0.f, std::min(m_area.width, m_area.height) * .025f);
    const float width = std::max(0.f, m_area.width - inset * 2.f);
    const float height = std::max(0.f, m_area.height - inset * 2.f);
    const float target = std::max(1.f, std::min(width * .25f, height * .24f));
    const float gap = std::max(1.f, std::min(width, height) * .018f);
    const float gridWidth = std::max(0.f, width - inset * 2.f);
    const auto columns = std::size_t{4};
    const float cellWidth = std::max(1.f, (gridWidth - gap * static_cast<float>(columns - 1)) / static_cast<float>(columns));
    const float cellHeight = std::min(cellWidth, target);

    m_catalog->content.clear();
    m_catalog->scrollTo(0.f);
    m_catalog->content.gap(gap).padding(LayoutInsets::all(inset));
    m_catalog->content.alignment(LayoutAlignment::Start);
    BlockComponentPtr row;
    std::size_t column = 0;
    for (std::size_t index = 0; index < m_state->products.size(); ++index) {
        if (m_selectedCategory&& m_state->products[index].categoryId != *m_selectedCategory) continue;
        if (column == 0) {
            row = std::make_shared<BlockComponent>(LayoutAxis::Horizontal);
            row->content.gap(gap).alignment(LayoutAlignment::Start);
            m_catalog->content.add(row, LayoutSize::fill(), LayoutSize::px(cellHeight));
        }
        auto& card = m_productCards[index];
        card->fontSize = static_cast<int>(std::clamp(std::min(cellWidth, cellHeight) * .105f, 12.f, 25.f));
        card->image = m_state->fallbackImage;
        row->content.add(card, LayoutSize::px(cellWidth), LayoutSize::px(cellHeight));
        column = (column + 1) % columns;
    }
}

void ProductCatalogScreen::layout() {
    const float unit = std::max(1.f, std::min(m_area.width, m_area.height));
    const float inset = unit * .025f;
    const float titleHeight = std::clamp(m_area.height * .12f, 48.f, 76.f);
    const float fontScale = std::clamp(unit / 420.f, .72f, 1.45f);
    m_title->fontSize = std::clamp(25.f * fontScale, 16.f, 36.f);
    const float actionsWidth = 68.f + 8.f + 40.f + 8.f + 38.f + 8.f + 38.f;
    m_title->area({m_area.x + inset + 76, m_area.y + inset, std::max(0.f, m_area.width - 2 * inset - actionsWidth - 76.f), titleHeight - inset});
    m_backButton->area({m_area.x + inset, m_area.y + inset, 68, 38});
    m_editButton->enabled = !m_productCards.empty();
    m_deleteButton->enabled = !m_productCards.empty();
    m_addButton->area({m_area.x + m_area.width - inset - 40, m_area.y + inset, 38, 38});
    m_editButton->area({m_area.x + m_area.width - inset - 40 - 8 - 38, m_area.y + inset, 38, 38});
    m_deleteButton->area({m_area.x + m_area.width - inset - 40 - 8 - 38 - 8 - 38, m_area.y + inset, 38, 38});
    const Rectangle listArea{
        m_area.x + inset, m_area.y + titleHeight,
        std::max(0.f, m_area.width - 2 * inset),
        std::max(0.f, m_area.height - titleHeight - inset)
    };
    m_catalog->area(listArea);
    m_emptyLabel->text = NexoBarImp::get()->text("products.empty", "No hay productos en esta categoría.");
    m_emptyLabel->fontSize = std::clamp(unit * .045f, 14.f, 23.f);
    m_emptyLabel->area(listArea);
}

void ProductCatalogScreen::update(float deltaSeconds) {
    layout();
    m_backButton->update(deltaSeconds);
    m_addButton->update(deltaSeconds);
    m_editButton->update(deltaSeconds);
    m_deleteButton->update(deltaSeconds);
    if (m_state->fallbackImage == nullptr) {
        const TexturePtr source = NexoBarImp::get()->assets().getTexture("nexobar-icon");
        Image image = LoadImageFromTexture(*source);
        if (image.data) {
            ImageAlphaCrop(&image, .1f);
            const Texture2D texture = LoadTextureFromImage(image);
            UnloadImage(image);
            if (texture.id)
                m_state->fallbackImage = TexturePtr(new Texture2D(texture), [](Texture2D* loaded) {
                    UnloadTexture(*loaded);
                    delete loaded;
                });
        }
    }
    const int width = static_cast<int>(m_area.width);
    const int height = static_cast<int>(m_area.height);
    if (width != m_lastWidth || height != m_lastHeight) {
        m_lastWidth = width;
        m_lastHeight = height;
        rebuildProductGrid();
    }
    m_catalog->update(deltaSeconds);
    if (m_pendingEdit) {
        const auto id = *m_pendingEdit;
        m_pendingEdit.reset();
        editRequested(id);
    }
    if (m_pendingDelete) {
        const auto id = *m_pendingDelete;
        m_pendingDelete.reset();
        deleteRequested(id);
    }
}

void ProductCatalogScreen::draw() {
    layout();
    m_backButton->draw();
    m_addButton->draw();
    m_editButton->draw();
    m_deleteButton->draw();
    m_title->draw();
    const bool hasProducts = std::ranges::any_of(m_state->products, [this](const auto& product) {
        return !m_selectedCategory || product.categoryId == *m_selectedCategory;
    });
    if (hasProducts) m_catalog->draw();
    else m_emptyLabel->draw();
}


