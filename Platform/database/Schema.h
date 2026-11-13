//
// Created by cadox8 on 23/10/2026.
//
#pragma once

#include "Codec.h"

#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace Nearon {

    // ================= COLUMN CONSTRAINTS =================

    /// Primary key. Integer keys become the SQLite rowid: inserting a record whose key is 0 lets SQLite assign it.
    struct PrimaryKey {};

    /// AUTOINCREMENT: rowids of deleted rows are never reused (recommended for documents that must not repeat IDs).
    struct AutoIncrement {};

    struct Unique {};

    /// SQL default expression, e.g. Default<"0"> or Default<"CURRENT_TIMESTAMP">. Needed to add NOT NULL columns
    /// to an existing table.
    template<FixedString Expression>
    struct Default {};

    template<FixedString Expression>
    struct Check {};

    template<FixedString Collation>
    struct Collate {};

    enum class FkAction { NoAction, Restrict, SetNull, SetDefault, Cascade };

    /// REFERENCES parent(column). ParentMember is the parent's member pointer, e.g. ForeignKey<&Category::id>.
    template<auto ParentMember, FkAction OnDelete = FkAction::Restrict, FkAction OnUpdate = FkAction::NoAction>
    struct ForeignKey {};

    // ================= TABLE CONSTRAINTS =================

    /// Composite UNIQUE(a, b, ...) constraint.
    template<auto... Members>
    struct UniqueKey {};

    /// CREATE INDEX IF NOT EXISTS name ON table(a, b, ...).
    template<FixedString Name, auto... Members>
    struct Index {};

    // ================= TRAITS =================

    template<typename>
    struct MemberTraits;

    template<typename C, typename M>
    struct MemberTraits<M C::*> {
        using class_type = C;
        using value_type = M;
    };

    template<auto Member>
    using MemberClass = typename MemberTraits<decltype(Member)>::class_type;

    template<FixedString Name, auto Member, typename... Constraints>
    struct Column;

    template<typename T>
    struct IsColumn : std::false_type {};

    template<FixedString Name, auto Member, typename... Constraints>
    struct IsColumn<Column<Name, Member, Constraints...>> : std::true_type {};

    /// Table lookup by record type. The table is found through ADL on nearonTableOf(Record*), which NEARON_TABLE and
    /// NEARON_TABLE_FRIEND declare next to the record.
    template<typename Record>
    using TableOf = decltype(nearonTableOf(static_cast<Record*>(nullptr)));

    template<typename Record>
    concept Persistable = requires { nearonTableOf(static_cast<Record*>(nullptr)); };

    template<typename Record> requires Persistable<Record>
    using KeyOf = typename TableOf<Record>::key_type;

    inline constexpr std::size_t npos = std::numeric_limits<std::size_t>::max();

    namespace detail {
        template<auto M, typename C>
        consteval bool isMember() {
            if constexpr (std::is_same_v<std::remove_cv_t<decltype(M)>, std::remove_cv_t<decltype(C::member)>>)
                return M == C::member;
            else
                return false;
        }

        template<typename Tuple>
        struct ColumnList;

        template<typename... Cs>
        struct ColumnList<std::tuple<Cs...>> {
            static constexpr std::size_t primaryKeys = (std::size_t{Cs::isPrimaryKey} + ... + 0);

            static consteval std::size_t primaryKeyIndex() {
                std::size_t index = 0, result = npos;
                ((Cs::isPrimaryKey && result == npos ? result = index : 0, ++index), ...);
                return result;
            }

            template<auto M>
            static consteval std::size_t indexOfMember() {
                std::size_t index = 0, result = npos;
                ((isMember<M, Cs>() && result == npos ? result = index : 0, ++index), ...);
                return result;
            }

            static consteval std::size_t indexOfName(const std::string_view name) {
                std::size_t index = 0, result = npos;
                ((Cs::name == name && result == npos ? result = index : 0, ++index), ...);
                return result;
            }
        };

        inline std::string_view fkActionSql(const FkAction action) {
            switch (action) {
                case FkAction::NoAction: return "NO ACTION";
                case FkAction::Restrict: return "RESTRICT";
                case FkAction::SetNull: return "SET NULL";
                case FkAction::SetDefault: return "SET DEFAULT";
                case FkAction::Cascade: return "CASCADE";
            }
            return "NO ACTION";
        }

        template<auto Member>
        std::string columnNameOf();

        template<typename T>
        struct ConstraintSql {
            static std::string sql() { return {}; }
        };

        template<>
        struct ConstraintSql<Unique> {
            static std::string sql() { return " UNIQUE"; }
        };

        template<FixedString E>
        struct ConstraintSql<Default<E>> {
            static std::string sql() { return " DEFAULT (" + std::string(E.view()) + ")"; }
        };

        template<FixedString E>
        struct ConstraintSql<Check<E>> {
            static std::string sql() { return " CHECK (" + std::string(E.view()) + ")"; }
        };

        template<FixedString E>
        struct ConstraintSql<Collate<E>> {
            static std::string sql() { return " COLLATE " + std::string(E.view()); }
        };

        template<auto ParentMember, FkAction OnDelete, FkAction OnUpdate>
        struct ConstraintSql<ForeignKey<ParentMember, OnDelete, OnUpdate>> {
            static std::string sql() {
                using Parent = TableOf<MemberClass<ParentMember>>;
                return " REFERENCES " + quoteIdentifier(Parent::name) + " (" + columnNameOf<ParentMember>() + ")"
                       + " ON DELETE " + std::string(fkActionSql(OnDelete))
                       + " ON UPDATE " + std::string(fkActionSql(OnUpdate));
            }
        };

        template<typename T>
        struct TableConstraintSql;

        template<auto... Members>
        struct TableConstraintSql<UniqueKey<Members...>> {
            static std::string sql() {
                std::string columns;
                ((columns += (columns.empty() ? "" : ", ") + columnNameOf<Members>()), ...);
                return "UNIQUE (" + columns + ")";
            }

            static std::string indexSql(std::string_view) { return {}; }
        };

        template<FixedString Name, auto... Members>
        struct TableConstraintSql<Index<Name, Members...>> {
            static std::string sql() { return {}; }

            static std::string indexSql(const std::string_view table) {
                std::string columns;
                ((columns += (columns.empty() ? "" : ", ") + columnNameOf<Members>()), ...);
                return "CREATE INDEX IF NOT EXISTS " + quoteIdentifier(Name.view()) + " ON " + quoteIdentifier(table)
                       + " (" + columns + ")";
            }
        };
    }

    // ================= COLUMN =================

    template<FixedString Name, auto Member, typename... Constraints>
    struct Column {
        using record_type = MemberClass<Member>;
        using value_type = typename MemberTraits<decltype(Member)>::value_type;
        using codec = Codec<value_type>;

        static_assert(Storable<value_type>, "Column type is not storable: specialise Nearon::ValueCodec for it");

        static constexpr auto member = Member;
        static constexpr auto fixedName = Name;
        static constexpr std::string_view name = fixedName.view();

        static constexpr bool isPrimaryKey = (std::is_same_v<Constraints, PrimaryKey> || ...);
        static constexpr bool isAutoIncrement = (std::is_same_v<Constraints, AutoIncrement> || ...);
        /// INTEGER PRIMARY KEY: alias of the rowid, assigned by SQLite when inserted as 0.
        static constexpr bool isRowId = isPrimaryKey && codec::type == SqlType::Integer;

        static_assert(!isAutoIncrement || isRowId, "AutoIncrement requires an integer primary key");
        static_assert(!isPrimaryKey || !codec::nullable, "Primary keys can not be optional");

        static std::string definition() {
            std::string sql = quoteIdentifier(name) + " " + std::string(sqlTypeName(codec::type));
            if constexpr (isPrimaryKey) {
                sql += " PRIMARY KEY";
                if constexpr (isAutoIncrement) sql += " AUTOINCREMENT";
            }
            if constexpr (!codec::nullable) sql += " NOT NULL";
            (sql += ... += detail::ConstraintSql<Constraints>::sql());
            return sql;
        }

        static Value read(const record_type& record) { return codec::encode(record.*member); }
        static void write(record_type& record, const Value& value) { record.*member = codec::decode(value); }
    };

    // ================= TABLE =================

    template<FixedString Name, typename Record, typename... Items>
    struct Table {
        using record_type = Record;

        using columns = decltype(std::tuple_cat(
            std::declval<std::conditional_t<IsColumn<Items>::value, std::tuple<Items>, std::tuple<>>>()...));
        using constraints = decltype(std::tuple_cat(
            std::declval<std::conditional_t<IsColumn<Items>::value, std::tuple<>, std::tuple<Items>>>()...));

        static constexpr auto fixedName = Name;
        static constexpr std::string_view name = fixedName.view();
        static constexpr std::size_t columnCount = std::tuple_size_v<columns>;

        static_assert(columnCount > 0, "A table needs at least one column");
        static_assert(detail::ColumnList<columns>::primaryKeys == 1, "A table needs exactly one PrimaryKey column");

        static constexpr std::size_t primaryKeyIndex = detail::ColumnList<columns>::primaryKeyIndex();

        using primary_key = std::tuple_element_t<primaryKeyIndex, columns>;
        using key_type = typename primary_key::value_type;

        template<std::size_t I>
        using column = std::tuple_element_t<I, columns>;

        template<auto M>
        static constexpr std::size_t indexOf() {
            constexpr std::size_t index = detail::ColumnList<columns>::template indexOfMember<M>();
            static_assert(index != npos, "Member is not mapped as a column of this table");
            return index;
        }

        template<FixedString ColumnName>
        static constexpr std::size_t indexOfName() {
            constexpr std::size_t index = detail::ColumnList<columns>::indexOfName(ColumnName.view());
            static_assert(index != npos, "Unknown column name for this table");
            return index;
        }

        /// Calls f(ColumnType{}, std::integral_constant<std::size_t, I>{}) for every column.
        template<typename F>
        static void forEachColumn(F&& f) {
            [&]<std::size_t... I>(std::index_sequence<I...>) {
                (f(column<I>{}, std::integral_constant<std::size_t, I>{}), ...);
            }(std::make_index_sequence<columnCount>{});
        }

        template<typename F>
        static void forEachConstraint(F&& f) {
            std::apply([&](auto... constraint) { (f(constraint), ...); }, constraints{});
        }

        static std::string quotedName() { return quoteIdentifier(name); }

        static std::string createSql() {
            std::string sql = "CREATE TABLE IF NOT EXISTS " + quotedName() + " (";
            bool first = true;
            forEachColumn([&](auto col, auto) {
                sql += (first ? "" : ", ") + decltype(col)::definition();
                first = false;
            });
            forEachConstraint([&](auto constraint) {
                if (const std::string part = detail::TableConstraintSql<decltype(constraint)>::sql(); !part.empty())
                    sql += ", " + part;
            });
            return sql + ")";
        }

        static std::vector<std::string> indexSql() {
            std::vector<std::string> statements;
            forEachConstraint([&](auto constraint) {
                if (std::string part = detail::TableConstraintSql<decltype(constraint)>::indexSql(name); !part.empty())
                    statements.push_back(std::move(part));
            });
            return statements;
        }
    };

    /// Record type of a table without instantiating it (so codecs can be declared after the table).
    template<typename T>
    struct RecordOf;

    template<FixedString Name, typename Record, typename... Items>
    struct RecordOf<Table<Name, Record, Items...>> {
        using type = Record;
    };

    namespace detail {
        template<auto Member>
        std::string columnNameOf() {
            using Owner = TableOf<MemberClass<Member>>;
            return quoteIdentifier(Owner::template column<Owner::template indexOf<Member>()>::name);
        }
    }
}

/**
 * Registers a table declared as an alias, next to its record (same namespace):
 *
 *   using ProductTable = Nearon::Table<"products", Product,
 *       Nearon::Column<"id", &Product::id, Nearon::PrimaryKey>,
 *       Nearon::Column<"name", &Product::name>>;
 *   NEARON_TABLE(ProductTable);
 */
#define NEARON_TABLE(TableType) TableType nearonTableOf(::Nearon::RecordOf<TableType>::type*)

/**
 * Declares the table inside a class, so private members can be mapped:
 *
 *   class Product {
 *       ProductId m_id; std::string m_name;
 *   public:
 *       NEARON_TABLE_FRIEND("products", Product,
 *           Nearon::Column<"id", &Product::m_id, Nearon::PrimaryKey>,
 *           Nearon::Column<"name", &Product::m_name>);
 *   };
 */
#define NEARON_TABLE_FRIEND(Name, Record, ...) \
    friend auto nearonTableOf(Record*) { return ::Nearon::Table<Name, Record, __VA_ARGS__>{}; }
