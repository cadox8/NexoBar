//
// Created by cadox8 on 23/10/2026.
//
#pragma once

#include "../export.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace Nearon {

    using Blob = std::vector<std::uint8_t>;

    /// A single SQLite value. Index order matches the SQLite storage classes.
    using Value = std::variant<std::nullptr_t, std::int64_t, double, std::string, Blob>;

    enum class SqlType { Integer, Real, Text, Blob };

    constexpr std::string_view sqlTypeName(const SqlType type) {
        switch (type) {
            case SqlType::Integer: return "INTEGER";
            case SqlType::Real: return "REAL";
            case SqlType::Text: return "TEXT";
            case SqlType::Blob: return "BLOB";
        }
        return "BLOB";
    }

    class DatabaseError : public std::runtime_error {
    public:
        explicit DatabaseError(const std::string& message, const int code = 0)
            : std::runtime_error(message), m_code(code) {}

        /// Extended SQLite result code (0 when the error does not come from SQLite).
        int code() const noexcept { return m_code; }

    private:
        int m_code;
    };

    /// UNIQUE, NOT NULL, FOREIGN KEY or CHECK violations.
    class ConstraintError : public DatabaseError {
    public:
        using DatabaseError::DatabaseError;
    };

    class NotFoundError : public DatabaseError {
    public:
        using DatabaseError::DatabaseError;
    };

    /// Compile-time string usable as a template argument: Table<"products", ...>.
    template<std::size_t N>
    struct FixedString {
        char value[N]{};

        constexpr FixedString(const char (&str)[N]) { std::copy_n(str, N, value); }

        constexpr std::string_view view() const { return {value, N - 1}; }
    };

    inline std::string quoteIdentifier(const std::string_view identifier) {
        std::string result = "\"";
        for (const char c : identifier) {
            if (c == '"') result += '"';
            result += c;
        }
        result += '"';
        return result;
    }
}
