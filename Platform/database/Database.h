//
// Created by cadox8 on 23/10/2026.
//
#pragma once

#include "Connection.h"
#include "Query.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <ranges>
#include <set>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace Nearon {

    /**
     * SQLite database. Every operation is driven from here using the record type:
     *
     *   auto db = std::make_shared<Nearon::Database>("nexobar.db");
     *   db->createTables<Category, Product>();
     *   ProductId id = db->insert<Product>(product);
     *   std::optional<Product> stored = db->select<Product>(id);
     *   db->update<Product>(*stored);
     *   db->remove<Product>(id);
     *
     * Records are plain classes/structs registered with NEARON_TABLE or NEARON_TABLE_FRIEND, and must be default
     * constructible.
     */
    class Database : public Connection {
    public:
        using Connection::Connection;

        // ================= SCHEMA =================

        /// CREATE TABLE IF NOT EXISTS + indexes, and adds columns missing from existing tables.
        template<Persistable... Records>
        void createTables() {
            Transaction tx(*this);
            (createTable<Records>(), ...);
            tx.commit();
        }

        template<Persistable T>
        void dropTable() {
            execute("DROP TABLE IF EXISTS " + TableOf<T>::quotedName());
        }

        template<Persistable T>
        static std::string schemaSql() { return TableOf<T>::createSql(); }

        // ================= INSERT =================

        /// Inserts the record and returns its key. Integer keys equal to 0 are assigned by SQLite and written back.
        template<Persistable T>
        KeyOf<T> insert(T& record) {
            std::lock_guard lock(mutex());
            KeyOf<T> key = insertRecord(record);
            TableOf<T>::primary_key::write(record, Codec<KeyOf<T>>::encode(key));
            return key;
        }

        template<Persistable T>
        KeyOf<T> insert(const T& record) {
            std::lock_guard lock(mutex());
            return insertRecord(record);
        }

        /// Inserts every record in a single transaction.
        template<Persistable T, std::ranges::range Range>
        std::vector<KeyOf<T>> insertMany(Range&& records) {
            Transaction tx(*this);
            std::vector<KeyOf<T>> keys;
            for (auto&& record : records)
                keys.push_back(insert<T>(record));
            tx.commit();
            return keys;
        }

        /// INSERT or, if the key already exists, UPDATE every column.
        template<Persistable T>
        void upsert(const T& record) {
            using Tb = TableOf<T>;
            using Pk = typename Tb::primary_key;
            if constexpr (Pk::isRowId) {
                if (std::get<std::int64_t>(Pk::read(record)) == 0)
                    throw DatabaseError("upsert requires a non-zero rowid; use insert for a new record");
            }
            std::string columns, placeholders, assignments;
            std::vector<Value> params;
            Tb::forEachColumn([&](auto column, auto) {
                using C = decltype(column);
                columns += (columns.empty() ? "" : ", ") + quoteIdentifier(C::name);
                placeholders += placeholders.empty() ? "?" : ", ?";
                params.push_back(C::read(record));
                if constexpr (!C::isPrimaryKey)
                    assignments += (assignments.empty() ? "" : ", ") + quoteIdentifier(C::name) + " = excluded." + quoteIdentifier(C::name);
            });
            std::string query = "INSERT INTO " + Tb::quotedName() + " (" + columns + ") VALUES (" + placeholders + ")"
                                + " ON CONFLICT (" + quoteIdentifier(Tb::primary_key::name) + ") DO ";
            query += assignments.empty() ? "NOTHING" : "UPDATE SET " + assignments;
            run(query, params);
        }

        // ================= SELECT =================

        template<Persistable T>
        std::optional<T> select(const KeyOf<T>& key) {
            return query<T>().where(keyField<T>() == key).first();
        }

        /// Like select() but throws NotFoundError when the record does not exist.
        template<Persistable T>
        T get(const KeyOf<T>& key) {
            if (auto record = select<T>(key)) return std::move(*record);
            throw NotFoundError("Record not found in table " + std::string(TableOf<T>::name));
        }

        template<Persistable T>
        std::vector<T> selectAll() { return query<T>().all(); }

        template<Persistable T>
        std::vector<T> selectWhere(Expr condition) { return query<T>().where(std::move(condition)).all(); }

        template<Persistable T>
        std::optional<T> selectFirst(Expr condition) { return query<T>().where(std::move(condition)).first(); }

        /// Query builder with where / orderBy / limit / offset / join / union.
        template<Persistable T>
        SelectQuery<T> query() { return SelectQuery<T>(*this); }

        template<Persistable A, Persistable B>
        auto join(const Expr& on) { return query<A>().template join<B>(on); }

        template<Persistable A, Persistable B>
        auto leftJoin(const Expr& on) { return query<A>().template leftJoin<B>(on); }

        template<Persistable T>
        bool exists(const KeyOf<T>& key) { return select<T>(key).has_value(); }

        template<Persistable T>
        std::size_t count(Expr condition = {}) { return query<T>().where(std::move(condition)).count(); }

        /// SUM of a numeric column (0 when there are no rows): db->sum<&Payment::amount>(condition).
        template<auto Member>
        auto sum(const Expr& condition = {}) {
            using F = std::remove_cvref_t<decltype(col<Member>)>;
            using Result = std::conditional_t<Codec<typename F::value_type>::type == SqlType::Real, double, std::int64_t>;
            std::string query = "SELECT COALESCE(SUM(" + F::sql() + "), 0) FROM " + F::table::quotedName();
            if (!condition.empty()) query += " WHERE " + condition.sql;
            Result result{};
            fetch(query, condition.params, [&](const Row& row) {
                if constexpr (std::is_same_v<Result, double>) result = detail::asReal(row[0]);
                else result = detail::asInteger(row[0]);
            });
            return result;
        }

        /// Raw query mapping each row to a tuple: db->queryRaw<std::string, std::int64_t>("SELECT name, price FROM ...").
        template<typename... Columns, typename... Args>
        std::vector<std::tuple<Columns...>> queryRaw(const std::string& sql, const Args&... args) {
            std::vector<std::tuple<Columns...>> rows;
            fetch(sql, {Codec<Args>::encode(args)...}, [&](const Row& row) {
                rows.push_back([&]<std::size_t... I>(std::index_sequence<I...>) {
                    return std::tuple<Columns...>{Codec<Columns>::decode(row[static_cast<int>(I)])...};
                }(std::index_sequence_for<Columns...>{}));
            });
            return rows;
        }

        /// First column of the first row, or std::nullopt.
        template<typename R, typename... Args>
        std::optional<R> scalar(const std::string& sql, const Args&... args) {
            std::optional<R> result;
            fetch(sql, {Codec<Args>::encode(args)...}, [&](const Row& row) {
                if (!result) result = Codec<R>::decode(row[0]);
            });
            return result;
        }

        // ================= UPDATE =================

        /// Updates every column by primary key. Returns false if the record does not exist.
        template<Persistable T>
        bool update(const T& record) {
            using Tb = TableOf<T>;
            std::string assignments;
            std::vector<Value> params;
            Tb::forEachColumn([&](auto column, auto) {
                using C = decltype(column);
                if constexpr (!C::isPrimaryKey) {
                    assignments += (assignments.empty() ? "" : ", ") + quoteIdentifier(C::name) + " = ?";
                    params.push_back(C::read(record));
                }
            });
            if (assignments.empty()) return exists<T>(record.*Tb::primary_key::member);
            params.push_back(Tb::primary_key::read(record));
            return run("UPDATE " + Tb::quotedName() + " SET " + assignments + " WHERE "
                       + quoteIdentifier(Tb::primary_key::name) + " = ?", params) > 0;
        }

        /// Update builder: db->update<Product>().set(col<&Product::price>, Cents{150}).where(...).exec().
        template<Persistable T>
        UpdateQuery<T> update() { return UpdateQuery<T>(*this); }

        // ================= DELETE =================

        template<Persistable T>
        bool remove(const KeyOf<T>& key) {
            return removeWhere<T>(keyField<T>() == key) > 0;
        }

        template<Persistable T> requires (!std::is_same_v<T, KeyOf<T>>)
        bool remove(const T& record) {
            return remove<T>(record.*TableOf<T>::primary_key::member);
        }

        /// Returns the number of deleted rows. An empty condition deletes every row.
        template<Persistable T>
        std::size_t removeWhere(const Expr& condition) {
            std::string query = "DELETE FROM " + TableOf<T>::quotedName();
            if (!condition.empty()) query += " WHERE " + condition.sql;
            return run(query, condition.params);
        }

        // ================= TRANSACTIONS =================

        Transaction transaction() { return Transaction(*this); }

        /// Runs f inside a transaction: commits if it returns, rolls back if it throws.
        template<typename F>
        auto transaction(F&& f) {
            Transaction tx(*this);
            if constexpr (std::is_void_v<std::invoke_result_t<F>>) {
                std::forward<F>(f)();
                tx.commit();
            } else {
                auto result = std::forward<F>(f)();
                tx.commit();
                return result;
            }
        }

    private:
        template<Persistable T>
        static constexpr auto keyField() {
            return Field<T, TableOf<T>::primaryKeyIndex>{};
        }

        template<Persistable T>
        KeyOf<T> insertRecord(const T& record) {
            using Tb = TableOf<T>;
            using Pk = typename Tb::primary_key;

            const Value key = Pk::read(record);
            const bool generated = Pk::isRowId && std::get<std::int64_t>(key) == 0;

            std::string columns, placeholders;
            std::vector<Value> params;
            Tb::forEachColumn([&](auto column, auto) {
                using C = decltype(column);
                if (C::isPrimaryKey && generated) return;
                columns += (columns.empty() ? "" : ", ") + quoteIdentifier(C::name);
                placeholders += placeholders.empty() ? "?" : ", ?";
                params.push_back(C::read(record));
            });

            std::string query = "INSERT INTO " + Tb::quotedName();
            query += columns.empty() ? " DEFAULT VALUES" : " (" + columns + ") VALUES (" + placeholders + ")";
            run(query, params);

            return Codec<KeyOf<T>>::decode(generated ? Value{lastInsertRowId()} : key);
        }

        template<Persistable T>
        void createTable() {
            using Tb = TableOf<T>;
            execute(Tb::createSql());

            std::set<std::string> existing;
            fetch("SELECT name FROM pragma_table_info(?)", {std::string(Tb::name)}, [&](const Row& row) {
                existing.insert(std::get<std::string>(row[0]));
            });

            Tb::forEachColumn([&](auto column, auto) {
                using C = decltype(column);
                if (existing.contains(std::string(C::name))) return;
                if (C::isPrimaryKey)
                    throw DatabaseError("Can not add primary key column " + std::string(C::name) + " to " + std::string(Tb::name));
                execute("ALTER TABLE " + Tb::quotedName() + " ADD COLUMN " + C::definition());
            });

            for (const std::string& index : Tb::indexSql())
                execute(index);
        }
    };
}
