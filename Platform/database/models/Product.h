#pragma once

#include "../hospitality/Types.h"
#include "../Orm.h"
#include "Category.h"
#include "Tax.h"

#include <string>

namespace nexobar {
    /// A fixed-price catalogue item. openPrice remains as a legacy database
    /// column so existing installations can be upgraded without rebuilding data.
    class Product {
    public:
        ProductId id{};
        std::string name;
        CategoryId categoryId{};
        TaxId taxId{};
        Cents price{};
        bool openPrice{false};
        bool active{true};
    };

    using ProductTable = Nearon::Table<"products", Product,
        Nearon::Column<"id", &Product::id, Nearon::PrimaryKey, Nearon::AutoIncrement>,
        Nearon::Column<"name", &Product::name, Nearon::Collate<"NOCASE">>,
        Nearon::Column<"category_id", &Product::categoryId, Nearon::ForeignKey<&Category::id>>,
        Nearon::Column<"tax_id", &Product::taxId, Nearon::ForeignKey<&Tax::id>>,
        Nearon::Column<"price_cents", &Product::price, Nearon::Check<"price_cents >= 0">>,
        Nearon::Column<"open_price", &Product::openPrice, Nearon::Default<"0">>,
        Nearon::Column<"active", &Product::active, Nearon::Default<"1">>,
        Nearon::Index<"idx_products_category", &Product::categoryId>>;
    NEARON_TABLE(ProductTable);
}
