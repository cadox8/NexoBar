//
// Created by cadox8 on 23/10/2026.
//
#pragma once

#include "../export.h"
#include "Value.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

struct sqlite3;
struct sqlite3_stmt;

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4251) // STL members of exported classes
#endif

namespace Nearon {

    struct OpenOptions {
        bool create = true;
        bool readOnly = false;
        bool foreignKeys = true;
        /// WAL journal: readers do not block the writer and survives power cuts better than the rollback journal.
        bool wal = true;
        std::chrono::milliseconds busyTimeout{5000};
    };

    /// Read-only view of the current result row.
    class PLATFORM_API Row {
    public:
        explicit Row(sqlite3_stmt* statement) : m_statement(statement) {}

        int size() const;

        Value operator[](int column) const;

    private:
        sqlite3_stmt* m_statement;
    };

    /**
     * Non-template core of the database: owns the sqlite3 handle, caches prepared statements and manages
     * (nested) transactions. Every operation is serialised with a recursive mutex, so a connection can be shared
     * between threads; a Transaction keeps the lock until it ends.
     */
    class PLATFORM_API Connection {
    public:
        explicit Connection(const std::filesystem::path& path, const OpenOptions& options = {});

        virtual ~Connection();

        Connection(const Connection&) = delete;
        Connection& operator=(const Connection&) = delete;

        /// Runs one or more SQL statements without parameters.
        void execute(std::string_view sql);

        /// Runs a statement and returns the number of changed rows.
        std::size_t run(const std::string& sql, const std::vector<Value>& params = {});

        /// Runs a query calling onRow for every result row.
        void fetch(const std::string& sql, const std::vector<Value>& params, const std::function<void(const Row&)>& onRow);

        std::int64_t lastInsertRowId() const;

        int userVersion();

        void setUserVersion(int version);

        bool inTransaction() const noexcept { return m_transactionDepth > 0; }

        std::recursive_mutex& mutex() noexcept { return m_mutex; }

        sqlite3* handle() const noexcept { return m_db; }

        const std::filesystem::path& path() const noexcept { return m_path; }

    private:
        friend class Transaction;

        sqlite3_stmt* prepare(const std::string& sql);

        void bind(sqlite3_stmt* statement, const std::vector<Value>& params);

        [[noreturn]] void fail(int code, std::string_view context) const;

        void beginTransaction();

        void commitTransaction();

        void rollbackTransaction();

        sqlite3* m_db = nullptr;
        std::filesystem::path m_path;
        std::recursive_mutex m_mutex;
        std::unordered_map<std::string, sqlite3_stmt*> m_statements;
        int m_transactionDepth = 0;
    };

    /**
     * RAII transaction. The outermost one runs BEGIN IMMEDIATE, nested ones use SAVEPOINTs.
     * Rolls back on destruction unless commit() was called.
     */
    class PLATFORM_API Transaction {
    public:
        explicit Transaction(Connection& connection);

        ~Transaction();

        Transaction(const Transaction&) = delete;
        Transaction& operator=(const Transaction&) = delete;

        void commit();

        void rollback();

        bool active() const noexcept { return m_active; }

    private:
        Connection& m_connection;
        std::unique_lock<std::recursive_mutex> m_lock;
        bool m_active = false;
    };
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif
