//
// Created by cadox8 on 23/10/2026.
//
#pragma once

#include "Connection.h"
#include "Expression.h"

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace Nearon {

    namespace detail {
        template<typename T>
        std::string selectList() {
            using Tb = TableOf<T>;
            std::string sql;
            Tb::forEachColumn([&]<typename T0>([[maybe_unused]] T0 column, auto) {
                if (!sql.empty()) sql += ", ";
                sql += Tb::quotedName() + "." + quoteIdentifier(T0::name);
            });
            return sql;
        }

        template<typename T>
        T readRecord(const Row& row, const int offset) {
            T record{};
            TableOf<T>::forEachColumn([&]<typename T0, typename T1>([[maybe_unused]] T0 column,
                                                                    [[maybe_unused]] T1 index) {
                T0::write(record, row[offset + static_cast<int>(T1::value)]);
            });
            return record;
        }

        inline void appendParams(std::vector<Value>& target, const std::vector<Value>& source) {
            target.insert(target.end(), source.begin(), source.end());
        }

        /// WHERE / ORDER BY / LIMIT / OFFSET shared by every select.
        struct Clauses {
            Expr where;
            std::vector<std::string> order;
            std::optional<std::int64_t> limit;
            std::optional<std::int64_t> offset;

            void render(std::string& sql, std::vector<Value>& params) const {
                if (!where.empty()) {
                    sql += " WHERE " + where.sql;
                    appendParams(params, where.params);
                }
                if (!order.empty()) {
                    sql += " ORDER BY ";
                    for (std::size_t i = 0; i < order.size(); ++i)
                        sql += (i ? ", " : "") + order[i];
                }
                if (limit || offset) {
                    sql += " LIMIT ?";
                    params.emplace_back(limit.value_or(-1));
                }
                if (offset) {
                    sql += " OFFSET ?";
                    params.emplace_back(*offset);
                }
            }
        };

        inline std::size_t countOf(Connection& connection, const std::string& sql, const std::vector<Value>& params) {
            std::size_t count = 0;
            connection.fetch("SELECT COUNT(*) FROM (" + sql + ")", params, [&](const Row& row) {
                count = static_cast<std::size_t>(std::get<std::int64_t>(row[0]));
            });
            return count;
        }
    }

    enum class JoinKind { From, Inner, Left };

    template<typename Record, JoinKind Kind>
    struct JoinPart {
        using record_type = Record;
        static constexpr JoinKind kind = Kind;
        /// LEFT JOIN rows without a match come back as std::nullopt.
        using result_type = std::conditional_t<Kind == JoinKind::Left, std::optional<Record>, Record>;
    };

    /**
     * SELECT over several tables. Each row is a std::tuple with one element per table, in join order:
     *   db->query<Order>().join<OrderLine>(col<&OrderLine::orderId> == col<&Order::id>).all()
     *   -> std::vector<std::tuple<Order, OrderLine>>
     */
    template<typename... Parts>
    class JoinQuery {
    public:
        using row_type = std::tuple<typename Parts::result_type...>;

        JoinQuery(Connection& connection, std::string joins, std::vector<Value> joinParams, detail::Clauses clauses)
            : m_connection(&connection), m_joins(std::move(joins)), m_joinParams(std::move(joinParams)),
              m_clauses(std::move(clauses)) {}

        template<Persistable R>
        JoinQuery<Parts..., JoinPart<R, JoinKind::Inner>> join(const Expr& on) const { return add<R, JoinKind::Inner>(on); }

        template<Persistable R>
        JoinQuery<Parts..., JoinPart<R, JoinKind::Left>> leftJoin(const Expr& on) const { return add<R, JoinKind::Left>(on); }

        JoinQuery& where(Expr condition) {
            m_clauses.where = std::move(m_clauses.where) && std::move(condition);
            return *this;
        }

        template<typename... Orders>
        JoinQuery& orderBy(const OrderBy& first, const Orders&... rest) {
            m_clauses.order.push_back(first.sql);
            (m_clauses.order.push_back(rest.sql), ...);
            return *this;
        }

        JoinQuery& limit(const std::int64_t count) {
            m_clauses.limit = count;
            return *this;
        }

        JoinQuery& offset(const std::int64_t count) {
            m_clauses.offset = count;
            return *this;
        }

        std::vector<row_type> all() const {
            std::vector<Value> params;
            const std::string query = render(params);
            std::vector<row_type> rows;
            m_connection->fetch(query, params, [&](const Row& row) { rows.push_back(readRow(row)); });
            return rows;
        }

        std::optional<row_type> first() const {
            JoinQuery copy = *this;
            copy.m_clauses.limit = 1;
            auto rows = copy.all();
            if (rows.empty()) return std::nullopt;
            return std::move(rows.front());
        }

        std::size_t count() const {
            std::vector<Value> params;
            const std::string query = render(params);
            return detail::countOf(*m_connection, query, params);
        }

        std::string sql() const {
            std::vector<Value> params;
            return render(params);
        }

    private:
        template<typename... Other>
        friend class JoinQuery;

        template<std::size_t I>
        using part = std::tuple_element_t<I, std::tuple<Parts...>>;

        static constexpr std::array<std::size_t, sizeof...(Parts)> offsets() {
            std::array<std::size_t, sizeof...(Parts)> result{};
            std::size_t offset = 0, index = 0;
            ((result[index++] = offset, offset += TableOf<typename Parts::record_type>::columnCount), ...);
            return result;
        }

        template<Persistable R, JoinKind Kind>
        JoinQuery<Parts..., JoinPart<R, Kind>> add(const Expr& on) const {
            std::string joins = m_joins + (Kind == JoinKind::Left ? " LEFT JOIN " : " INNER JOIN ")
                                + TableOf<R>::quotedName() + " ON " + on.sql;
            std::vector<Value> params = m_joinParams;
            detail::appendParams(params, on.params);
            return {*m_connection, std::move(joins), std::move(params), m_clauses};
        }

        std::string render(std::vector<Value>& params) const {
            std::string columns;
            ((columns += (columns.empty() ? "" : ", ") + detail::selectList<typename Parts::record_type>()), ...);
            std::string query = "SELECT " + columns + " FROM " + TableOf<typename part<0>::record_type>::quotedName() + m_joins;
            params = m_joinParams;
            m_clauses.render(query, params);
            return query;
        }

        template<typename P>
        static typename P::result_type readPart(const Row& row, const int offset) {
            using R = typename P::record_type;
            if constexpr (P::kind == JoinKind::Left) {
                constexpr int pk = static_cast<int>(TableOf<R>::primaryKeyIndex);
                if (std::holds_alternative<std::nullptr_t>(row[offset + pk])) return std::nullopt;
            }
            return detail::readRecord<R>(row, offset);
        }

        row_type readRow(const Row& row) const {
            constexpr auto offset = offsets();
            return [&]<std::size_t... I>(std::index_sequence<I...>) {
                return row_type{readPart<part<I>>(row, static_cast<int>(offset[I]))...};
            }(std::index_sequence_for<Parts...>{});
        }

        Connection* m_connection;
        std::string m_joins;
        std::vector<Value> m_joinParams;
        detail::Clauses m_clauses;
    };

    /**
     * SELECT over one table:
     *   db->query<Product>().where(col<&Product::active> == true).orderBy(col<&Product::name>.asc()).limit(20).all()
     */
    template<Persistable T>
    class SelectQuery {
    public:
        explicit SelectQuery(Connection& connection) : m_connection(&connection) {}

        /// Several where() calls are combined with AND.
        SelectQuery& where(Expr condition) {
            m_clauses.where = std::move(m_clauses.where) && std::move(condition);
            return *this;
        }

        template<typename... Orders>
        SelectQuery& orderBy(const OrderBy& first, const Orders&... rest) {
            m_clauses.order.push_back(first.sql);
            (m_clauses.order.push_back(rest.sql), ...);
            return *this;
        }

        SelectQuery& limit(const std::int64_t count) {
            m_clauses.limit = count;
            return *this;
        }

        SelectQuery& offset(const std::int64_t count) {
            m_clauses.offset = count;
            return *this;
        }

        /// UNION (without duplicates) with another query over the same table.
        SelectQuery& unionWith(const SelectQuery& other) {
            if (m_connection != other.m_connection)
                throw DatabaseError("UNION queries must use the same database connection");
            m_unions.emplace_back(" UNION ", other);
            return *this;
        }

        SelectQuery& unionAll(const SelectQuery& other) {
            if (m_connection != other.m_connection)
                throw DatabaseError("UNION queries must use the same database connection");
            m_unions.emplace_back(" UNION ALL ", other);
            return *this;
        }

        template<Persistable R>
        JoinQuery<JoinPart<T, JoinKind::From>, JoinPart<R, JoinKind::Inner>> join(const Expr& on) const {
            return toJoin().template join<R>(on);
        }

        template<Persistable R>
        JoinQuery<JoinPart<T, JoinKind::From>, JoinPart<R, JoinKind::Left>> leftJoin(const Expr& on) const {
            return toJoin().template leftJoin<R>(on);
        }

        std::vector<T> all() const {
            std::vector<Value> params;
            const std::string query = render(params);
            std::vector<T> records;
            m_connection->fetch(query, params, [&](const Row& row) { records.push_back(detail::readRecord<T>(row, 0)); });
            return records;
        }

        std::optional<T> first() const {
            std::vector<Value> params;
            std::string query;
            if (m_unions.empty() && !m_clauses.limit) {
                SelectQuery copy = *this;
                copy.m_clauses.limit = 1;
                query = copy.render(params);
            } else {
                query = "SELECT * FROM (" + render(params) + ") LIMIT 1";
            }
            std::optional<T> record;
            m_connection->fetch(query, params, [&](const Row& row) { record = detail::readRecord<T>(row, 0); });
            return record;
        }

        std::size_t count() const {
            std::vector<Value> params;
            const std::string query = render(params);
            return detail::countOf(*m_connection, query, params);
        }

        bool exists() const { return first().has_value(); }

        std::string sql() const {
            std::vector<Value> params;
            return render(params);
        }

    private:
        JoinQuery<JoinPart<T, JoinKind::From>> toJoin() const {
            if (!m_unions.empty()) throw DatabaseError("UNION queries can not be joined");
            return {*m_connection, {}, {}, m_clauses};
        }

        std::string render(std::vector<Value>& params) const {
            std::string query = "SELECT " + detail::selectList<T>() + " FROM " + TableOf<T>::quotedName();
            m_clauses.render(query, params);
            if (m_unions.empty()) return query;

            query = "SELECT * FROM (" + query + ")";
            for (const auto& [op, other] : m_unions)
                query += op + "SELECT * FROM (" + other.render(params) + ")";
            return query;
        }

        Connection* m_connection;
        detail::Clauses m_clauses;
        std::vector<std::pair<std::string, SelectQuery>> m_unions;
    };

    /**
     * UPDATE with conditions:
     *   db->update<Product>().set(col<&Product::active>, false).where(col<&Product::categoryId> == id).exec()
     */
    template<Persistable T>
    class UpdateQuery {
    public:
        explicit UpdateQuery(Connection& connection) : m_connection(&connection) {}

        template<FieldType F, typename V>
        UpdateQuery& set(const F&, const V& value) {
            static_assert(std::is_same_v<typename F::record_type, T>, "Field belongs to another table");
            using value_type = typename F::value_type;
            static_assert(std::convertible_to<const V&, value_type>, "Value type does not match the column type");
            m_assignments.push_back(quoteIdentifier(F::column::name) + " = ?");
            m_params.push_back(Codec<value_type>::encode(static_cast<value_type>(value)));
            return *this;
        }

        /// Raw assignment, e.g. set(raw("\"stock\" = \"stock\" - ?", 1)).
        UpdateQuery& set(const Expr& assignment) {
            m_assignments.push_back(assignment.sql);
            detail::appendParams(m_params, assignment.params);
            return *this;
        }

        UpdateQuery& where(Expr condition) {
            m_where = std::move(m_where) && std::move(condition);
            return *this;
        }

        /// Returns the number of updated rows.
        std::size_t exec() const {
            if (m_assignments.empty()) throw DatabaseError("UPDATE without assignments");
            std::string query = "UPDATE " + TableOf<T>::quotedName() + " SET ";
            for (std::size_t i = 0; i < m_assignments.size(); ++i)
                query += (i ? ", " : "") + m_assignments[i];
            std::vector<Value> params = m_params;
            if (!m_where.empty()) {
                query += " WHERE " + m_where.sql;
                detail::appendParams(params, m_where.params);
            }
            return m_connection->run(query, params);
        }

    private:
        Connection* m_connection;
        std::vector<std::string> m_assignments;
        std::vector<Value> m_params;
        Expr m_where;
    };
}
