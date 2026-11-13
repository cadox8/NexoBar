//
// Created by cadox8 on 23/10/2026.
//
#pragma once

#include "Schema.h"

#include <concepts>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

namespace Nearon {

    /// SQL fragment plus its bound parameters. An empty Expr means "no condition".
    struct Expr {
        std::string sql;
        std::vector<Value> params;

        bool empty() const noexcept { return sql.empty(); }
    };

    /// Raw SQL condition with '?' placeholders: raw("price > ? * 2", 100).
    template<typename... Args>
    Expr raw(std::string sql, const Args&... args) {
        return {std::move(sql), {Codec<Args>::encode(args)...}};
    }

    inline Expr operator&&(Expr lhs, Expr rhs) {
        if (lhs.empty()) return rhs;
        if (rhs.empty()) return lhs;
        lhs.sql = "(" + lhs.sql + ") AND (" + rhs.sql + ")";
        lhs.params.insert(lhs.params.end(), std::make_move_iterator(rhs.params.begin()),
                          std::make_move_iterator(rhs.params.end()));
        return lhs;
    }

    inline Expr operator||(Expr lhs, Expr rhs) {
        if (lhs.empty()) return rhs;
        if (rhs.empty()) return lhs;
        lhs.sql = "(" + lhs.sql + ") OR (" + rhs.sql + ")";
        lhs.params.insert(lhs.params.end(), std::make_move_iterator(rhs.params.begin()),
                          std::make_move_iterator(rhs.params.end()));
        return lhs;
    }

    inline Expr operator!(Expr expr) {
        if (expr.empty()) return expr;
        expr.sql = "NOT (" + expr.sql + ")";
        return expr;
    }

    struct OrderBy {
        std::string sql;
    };

    /// Reference to a mapped column, used to build conditions: col<&Product::price> > Cents{100}.
    template<typename Record, std::size_t Index>
    struct Field {
        static constexpr bool isField = true;

        using record_type = Record;
        using table = TableOf<Record>;
        using column = typename table::template column<Index>;
        using value_type = typename column::value_type;
        /// Optional columns are compared against their contained type.
        using compare_type = UnwrapOptionalT<value_type>;

        static std::string sql() { return table::quotedName() + "." + quoteIdentifier(column::name); }

        template<std::convertible_to<compare_type> V>
        static Value encode(const V& value) { return Codec<compare_type>::encode(static_cast<compare_type>(value)); }

        Expr isNull() const { return {sql() + " IS NULL", {}}; }
        Expr isNotNull() const { return {sql() + " IS NOT NULL", {}}; }

        Expr like(std::string pattern) const { return {sql() + " LIKE ?", {Value{std::move(pattern)}}}; }

        template<std::convertible_to<compare_type> V>
        Expr between(const V& low, const V& high) const {
            return {sql() + " BETWEEN ? AND ?", {encode(low), encode(high)}};
        }

        template<typename Range>
        Expr in(const Range& values) const {
            Expr expr{sql() + " IN (", {}};
            for (const auto& value : values) {
                expr.sql += expr.params.empty() ? "?" : ", ?";
                expr.params.push_back(encode(value));
            }
            if (expr.params.empty()) return {"0", {}};
            expr.sql += ")";
            return expr;
        }

        Expr in(std::initializer_list<compare_type> values) const { return in<std::initializer_list<compare_type>>(values); }

        OrderBy asc() const { return {sql() + " ASC"}; }
        OrderBy desc() const { return {sql() + " DESC"}; }
    };

    template<typename T>
    concept FieldType = requires { requires T::isField; };

    /// Column reference by member pointer (public members): col<&Product::price>.
    template<auto Member>
    inline constexpr Field<MemberClass<Member>, TableOf<MemberClass<Member>>::template indexOf<Member>()> col{};

    /// Column reference by column name (also works for private members): field<Product, "price">.
    template<typename Record, FixedString Name>
    inline constexpr Field<Record, TableOf<Record>::template indexOfName<Name>()> field{};

    namespace detail {
        template<FieldType F, typename V>
        Expr compare(const char* op, const V& value) {
            if constexpr (FieldType<V>) {
                return {F::sql() + " " + op + " " + V::sql(), {}};
            } else {
                static_assert(std::convertible_to<const V&, typename F::compare_type>,
                              "Value type does not match the column type");
                return {F::sql() + " " + op + " ?", {F::encode(value)}};
            }
        }
    }

    template<FieldType F, typename V>
    Expr operator==(const F&, const V& value) { return detail::compare<F>("=", value); }

    template<FieldType F, typename V>
    Expr operator!=(const F&, const V& value) { return detail::compare<F>("<>", value); }

    template<FieldType F, typename V>
    Expr operator<(const F&, const V& value) { return detail::compare<F>("<", value); }

    template<FieldType F, typename V>
    Expr operator<=(const F&, const V& value) { return detail::compare<F>("<=", value); }

    template<FieldType F, typename V>
    Expr operator>(const F&, const V& value) { return detail::compare<F>(">", value); }

    template<FieldType F, typename V>
    Expr operator>=(const F&, const V& value) { return detail::compare<F>(">=", value); }
}
