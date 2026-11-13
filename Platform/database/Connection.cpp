//
// Created by cadox8 on 23/10/2026.
//
#include "Connection.h"

#include <sqlite3.h>

#include <string>
#include <type_traits>

namespace Nearon {

    namespace {
        constexpr std::size_t MaxCachedStatements = 256;

        bool isConstraintError(const int code) {
            return (code & 0xFF) == SQLITE_CONSTRAINT;
        }

        std::string savepointName(const int depth) {
            return "nearon_sp_" + std::to_string(depth);
        }
    }

    // ================= ROW =================

    int Row::size() const {
        return sqlite3_column_count(m_statement);
    }

    Value Row::operator[](const int column) const {
        switch (sqlite3_column_type(m_statement, column)) {
            case SQLITE_INTEGER:
                return static_cast<std::int64_t>(sqlite3_column_int64(m_statement, column));
            case SQLITE_FLOAT:
                return sqlite3_column_double(m_statement, column);
            case SQLITE_TEXT: {
                const auto* text = reinterpret_cast<const char*>(sqlite3_column_text(m_statement, column));
                return std::string(text, static_cast<std::size_t>(sqlite3_column_bytes(m_statement, column)));
            }
            case SQLITE_BLOB: {
                const auto* data = static_cast<const std::uint8_t*>(sqlite3_column_blob(m_statement, column));
                const int size = sqlite3_column_bytes(m_statement, column);
                if (size == 0) return Blob{};
                return Blob(data, data + size);
            }
            default:
                return nullptr;
        }
    }

    // ================= CONNECTION =================

    Connection::Connection(const std::filesystem::path& path, const OpenOptions& options) : m_path(path) {
        int flags = SQLITE_OPEN_FULLMUTEX | SQLITE_OPEN_URI;
        flags |= options.readOnly ? SQLITE_OPEN_READONLY : SQLITE_OPEN_READWRITE;
        if (options.create && !options.readOnly) flags |= SQLITE_OPEN_CREATE;

        const std::u8string utf8 = path.u8string();
        const std::string file(utf8.begin(), utf8.end());

        if (const int code = sqlite3_open_v2(file.c_str(), &m_db, flags, nullptr); code != SQLITE_OK) {
            const std::string message = m_db ? sqlite3_errmsg(m_db) : sqlite3_errstr(code);
            sqlite3_close_v2(m_db);
            m_db = nullptr;
            throw DatabaseError("Can not open database '" + file + "': " + message, code);
        }

        sqlite3_extended_result_codes(m_db, 1);
        sqlite3_busy_timeout(m_db, static_cast<int>(options.busyTimeout.count()));

        if (options.foreignKeys) execute("PRAGMA foreign_keys = ON");
        if (options.wal && !options.readOnly && file != ":memory:") {
            execute("PRAGMA journal_mode = WAL");
            // Acknowledged account and fiscal transactions must remain durable
            // even if the process or the machine stops abruptly.
            execute("PRAGMA synchronous = FULL");
        }
    }

    Connection::~Connection() {
        for (const auto& [sql, statement] : m_statements)
            sqlite3_finalize(statement);
        sqlite3_close_v2(m_db);
    }

    void Connection::fail(const int code, const std::string_view context) const {
        std::string message = std::string(context) + ": " + sqlite3_errmsg(m_db);
        if (isConstraintError(code)) throw ConstraintError(message, code);
        throw DatabaseError(message, code);
    }

    void Connection::execute(const std::string_view sql) {
        std::lock_guard lock(m_mutex);
        char* error = nullptr;
        const std::string text(sql);
        if (const int code = sqlite3_exec(m_db, text.c_str(), nullptr, nullptr, &error); code != SQLITE_OK) {
            std::string message = error ? error : sqlite3_errstr(code);
            sqlite3_free(error);
            message = "SQL error in '" + text + "': " + message;
            if (isConstraintError(code)) throw ConstraintError(message, code);
            throw DatabaseError(message, code);
        }
    }

    sqlite3_stmt* Connection::prepare(const std::string& sql) {
        if (const auto it = m_statements.find(sql); it != m_statements.end()) return it->second;

        if (m_statements.size() >= MaxCachedStatements) {
            for (const auto& [text, statement] : m_statements)
                sqlite3_finalize(statement);
            m_statements.clear();
        }

        sqlite3_stmt* statement = nullptr;
        if (const int code = sqlite3_prepare_v3(m_db, sql.c_str(), static_cast<int>(sql.size()),
                                                SQLITE_PREPARE_PERSISTENT, &statement, nullptr); code != SQLITE_OK)
            fail(code, "Can not prepare '" + sql + "'");

        m_statements.emplace(sql, statement);
        return statement;
    }

    void Connection::bind(sqlite3_stmt* statement, const std::vector<Value>& params) {
        if (static_cast<int>(params.size()) != sqlite3_bind_parameter_count(statement))
            throw DatabaseError("Wrong number of parameters for '" + std::string(sqlite3_sql(statement)) + "'");

        for (int i = 0; i < static_cast<int>(params.size()); ++i) {
            const int index = i + 1;
            const int code = std::visit([&]<typename T>(const T& value) {
                if constexpr (std::is_same_v<T, std::nullptr_t>)
                    return sqlite3_bind_null(statement, index);
                else if constexpr (std::is_same_v<T, std::int64_t>)
                    return sqlite3_bind_int64(statement, index, value);
                else if constexpr (std::is_same_v<T, double>)
                    return sqlite3_bind_double(statement, index, value);
                else if constexpr (std::is_same_v<T, std::string>)
                    return sqlite3_bind_text64(statement, index, value.data(), value.size(), SQLITE_TRANSIENT, SQLITE_UTF8);
                else if (value.empty())
                    return sqlite3_bind_zeroblob(statement, index, 0);
                else
                    return sqlite3_bind_blob64(statement, index, value.data(), value.size(), SQLITE_TRANSIENT);
            }, params[i]);

            if (code != SQLITE_OK) fail(code, "Can not bind parameter");
        }
    }

    std::size_t Connection::run(const std::string& sql, const std::vector<Value>& params) {
        std::lock_guard lock(m_mutex);
        fetch(sql, params, [](const Row&) {});
        return static_cast<std::size_t>(sqlite3_changes64(m_db));
    }

    void Connection::fetch(const std::string& sql, const std::vector<Value>& params,
                           const std::function<void(const Row&)>& onRow) {
        std::lock_guard lock(m_mutex);
        sqlite3_stmt* statement = prepare(sql);

        struct Reset {
            sqlite3_stmt* statement;

            ~Reset() {
                sqlite3_reset(statement);
                sqlite3_clear_bindings(statement);
            }
        } reset{statement};

        bind(statement, params);

        const Row row(statement);
        while (true) {
            const int code = sqlite3_step(statement);
            if (code == SQLITE_DONE) break;
            if (code != SQLITE_ROW) fail(code, "SQL error in '" + sql + "'");
            onRow(row);
        }
    }

    std::int64_t Connection::lastInsertRowId() const {
        return sqlite3_last_insert_rowid(m_db);
    }

    int Connection::userVersion() {
        int version = 0;
        fetch("PRAGMA user_version", {}, [&](const Row& row) {
            version = static_cast<int>(std::get<std::int64_t>(row[0]));
        });
        return version;
    }

    void Connection::setUserVersion(const int version) {
        execute("PRAGMA user_version = " + std::to_string(version));
    }

    void Connection::beginTransaction() {
        execute(m_transactionDepth == 0 ? std::string("BEGIN IMMEDIATE") : "SAVEPOINT " + savepointName(m_transactionDepth));
        ++m_transactionDepth;
    }

    void Connection::commitTransaction() {
        --m_transactionDepth;
        try {
            execute(m_transactionDepth == 0 ? std::string("COMMIT") : "RELEASE " + savepointName(m_transactionDepth));
        } catch (...) {
            ++m_transactionDepth;
            throw;
        }
    }

    void Connection::rollbackTransaction() {
        if (m_transactionDepth <= 0) throw DatabaseError("No active transaction to roll back");
        if (m_transactionDepth == 1) {
            execute("ROLLBACK");
        } else {
            const std::string name = savepointName(m_transactionDepth - 1);
            execute("ROLLBACK TO " + name + "; RELEASE " + name);
        }
        --m_transactionDepth;
    }

    // ================= TRANSACTION =================

    Transaction::Transaction(Connection& connection) : m_connection(connection), m_lock(connection.mutex()) {
        m_connection.beginTransaction();
        m_active = true;
    }

    Transaction::~Transaction() {
        if (!m_active) return;
        try {
            m_connection.rollbackTransaction();
        } catch (...) {
            // Destructors must not throw; SQLite rolls back by itself if the statement failed.
        }
    }

    void Transaction::commit() {
        if (!m_active) throw DatabaseError("Transaction already finished");
        m_connection.commitTransaction();
        m_active = false;
    }

    void Transaction::rollback() {
        if (!m_active) throw DatabaseError("Transaction already finished");
        m_connection.rollbackTransaction();
        m_active = false;
    }
}
