#include "CategoriesCatalogScreen.h"

#include "../../../NexoBar.h"
#include "../../components/layout/BlockComponent.h"
#include "../../components/button/Button.h"

#include <algorithm>
#include <utility>

CategoriesCatalogScreen::CategoriesCatalogScreen(std::shared_ptr<ProductState> state)
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

    m_addButton = std::make_shared<Button>();
    m_addButton->icon = ButtonIcon::Plus;
    m_editButton = std::make_shared<Button>(app->text("categories.edit"));
    m_deleteButton = std::make_shared<Button>(app->text("categories.delete"));
    for (auto b: {m_addButton, m_editButton, m_deleteButton}) {
        b->font = m_font;
        b->fontSize = 15;
        b->roundness = .18f;
        b->background = Color{39, 50, 66, 255};
        b->hoverBackground = Color{57, 73, 94, 255};
        b->textColor = RAYWHITE;
        b->padding = LayoutInsets::symmetric(7, 4);
    }
    m_addButton->onClick([this] { addRequested(); });
    m_addButton->enabled = m_state->hasPermission("manage_products");
    m_editButton->image = app->assets().getTexture("icon-edit");
    m_deleteButton->image = app->assets().getTexture("icon-delete");
    m_editButton->text.clear();
    m_deleteButton->text.clear();
    m_editButton->onClick([this] { setActionMode(m_actionMode == ActionMode::Edit ? ActionMode::Normal : ActionMode::Edit); });
    m_deleteButton->onClick([this] { setActionMode(m_actionMode == ActionMode::Delete ? ActionMode::Normal : ActionMode::Delete); });
    refresh();
}

void CategoriesCatalogScreen::setActionMode(ActionMode mode) {
    m_actionMode = mode;
    m_editButton->background = mode == ActionMode::Edit ? Color{224, 171, 53, 255} : Color{39, 50, 66, 255};
    m_editButton->hoverBackground = mode == ActionMode::Edit ? Color{240, 190, 71, 255} : Color{57, 73, 94, 255};
    m_deleteButton->background = mode == ActionMode::Delete ? Color{190, 63, 70, 255} : Color{39, 50, 66, 255};
    m_deleteButton->hoverBackground = mode == ActionMode::Delete ? Color{212, 77, 83, 255} : Color{57, 73, 94, 255};
}

void CategoriesCatalogScreen::refresh() {
    NexoBarImp app;
    m_categoryCards.clear();
    for (std::size_t i = 0; i < m_state->categories.size(); ++i) {
        const auto& c = m_state->categories[i];
        auto card = std::make_shared<ProductCard>(c.name, "", [this,i] {
            const auto id = m_state->categories[i].id;
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
            m_pendingCategory = id;
        });
        card->showImage = true;
        card->outOfStockLabel = app->text("products.out_of_stock");
        card->image = m_state->fallbackImage;
        card->font = m_font;
        card->background = Color{36, 44, 57, 255};
        card->hoverBackground = Color{48, 61, 78, 255};
        m_categoryCards.push_back(std::move(card));
    }
}

void CategoriesCatalogScreen::layout() {
    const float unit = std::max(1.f, std::min(m_area.width, m_area.height));
    const float inset = unit * .035f;
    const float heading = std::clamp(m_area.height * .12f, 48.f, 76.f);
    const float fontScale = std::clamp(unit / 420.f, .72f, 1.45f);
    NexoBarImp nexo;
    m_title->text = nexo->text("categories.title");
    m_title->fontSize = std::clamp(26.f * fontScale, 16.f, 38.f);
    m_editButton->enabled = m_state->hasPermission("manage_products") && !m_categoryCards.empty();
    m_deleteButton->enabled = m_state->hasPermission("manage_products") && !m_categoryCards.empty();
    const float actionsWidth = 40.f + 8.f + 38.f + 8.f + 38.f;
    m_title->area({m_area.x + inset, m_area.y + inset, std::max(0.f, m_area.width - 2 * inset - actionsWidth), heading - inset});
    m_addButton->area({m_area.x + m_area.width - inset - 40, m_area.y + inset, 38, 38});
    m_editButton->area({m_area.x + m_area.width - inset - 40 - 8 - 38, m_area.y + inset, 38, 38});
    m_deleteButton->area({m_area.x + m_area.width - inset - 40 - 8 - 38 - 8 - 38, m_area.y + inset, 38, 38});
    const Rectangle listArea{
        m_area.x + inset, m_area.y + heading,
        std::max(0.f, m_area.width - 2 * inset),
        std::max(0.f, m_area.height - heading - inset)
    };
    m_catalog->area(listArea);
    const float gap = std::max(6.f, unit * .018f);
    const std::size_t columns = 3;
    const float side = std::max(1.f, std::min((listArea.width - gap * 2.f) / 3.f, listArea.height * .27f));
    const float cell = std::max(1.f,
        (listArea.width - gap * static_cast<float>(columns - 1)) /
        static_cast<float>(columns));
    const float square = std::max(1.f, std::min(cell, side));
    m_catalog->content.clear();
    m_catalog->content.gap(gap).padding(LayoutInsets::all(gap));
    BlockComponentPtr row;
    std::size_t column = 0;
    for (std::size_t i = 0; i < m_categoryCards.size(); ++i) {
        auto& card = m_categoryCards[i];
        card->fontSize = static_cast<int>(std::clamp(square * .16f, 18.f, 27.f));
        card->image = m_state->fallbackImage;
        if (column == 0) {
            row = std::make_shared<BlockComponent>(LayoutAxis::Horizontal);
            row->content.gap(gap).alignment(LayoutAlignment::Start);
            m_catalog->content.add(row, LayoutSize::fill(), LayoutSize::px(square));
        }
        row->content.add(card, LayoutSize::px(square), LayoutSize::px(square));
        column = (column + 1) % columns;
    }
    m_emptyLabel->text = nexo->text("categories.empty");
    m_emptyLabel->fontSize = std::clamp(unit * .045f, 14.f, 23.f);
    m_emptyLabel->area(listArea);
}

void CategoriesCatalogScreen::update(float deltaSeconds) {
    if (!m_state->fallbackImage) {
        NexoBarImp nexo;
        const auto source = nexo->assets().getTexture("nexobar-icon");
        Image image = LoadImageFromTexture(*source);
        if (image.data) {
            ImageAlphaCrop(&image, .1f);
            const auto texture = LoadTextureFromImage(image);
            UnloadImage(image);
            if (texture.id)
                m_state->fallbackImage = TexturePtr(new Texture2D(texture), [](Texture2D* value) {
                    UnloadTexture(*value);
                    delete value;
                });
        }
    }
    layout();
    m_addButton->update(deltaSeconds);
    m_editButton->update(deltaSeconds);
    m_deleteButton->update(deltaSeconds);
    m_catalog->update(deltaSeconds);
    if (m_pendingCategory) {
        const auto id = *m_pendingCategory;
        m_pendingCategory.reset();
        categorySelected(id);
    }
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

void CategoriesCatalogScreen::draw() {
    layout();
    m_addButton->draw();
    m_editButton->draw();
    m_deleteButton->draw();
    m_title->draw();
    if (m_categoryCards.empty()) m_emptyLabel->draw();
    else m_catalog->draw();
}


