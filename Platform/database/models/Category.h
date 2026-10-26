#pragma once

#include "../hospitality/Types.h"
#include "../Orm.h"

#include <optional>
#include <string>

namespace nexobar {
    class Category {
    public:
        CategoryId id{};
        std::string name;
        std::optional<CategoryId> parentId;
        std::string icon;
        bool active{true};
    };

    using CategoryTable = Nearon::Table<"categories", Category,
        Nearon::Column<"id", &Category::id, Nearon::PrimaryKey, Nearon::AutoIncrement>,
        Nearon::Column<"name", &Category::name, Nearon::Collate<"NOCASE"> >,
        Nearon::Column<"parent_id", &Category::parentId, Nearon::ForeignKey<&Category::id, Nearon::FkAction::SetNull> >,
        Nearon::Column<"icon", &Category::icon, Nearon::Default<"'logo.png'"> >,
        Nearon::Column<"active", &Category::active, Nearon::Default<"1"> > >;
    NEARON_TABLE(CategoryTable);
}
