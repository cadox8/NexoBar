//
// Created by cadox8 on 23/10/2026.
//
#pragma once

#include "Value.h"
#include "../NamedType/named_type.hpp"

#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <concepts>
#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>

namespace Nearon {

    /**
     * Customisation point for user types. Specialise it to store a type through one of the supported types:
     *
     *   template<> struct Nearon::ValueCodec<Money> {
     *       using storage_type = std::int64_t;
     *       static constexpr bool enabled = true;
     *       static storage_type encode(const Money& v) { return v.cents; }
     *       static Money decode(storage_type v) { return {v}; }
     *   };
     *
     * A ValueCodec takes precedence over the built-in enum mappings (NamedEnum as TEXT, other enums as INTEGER).
     */
    template<typename T>
    struct ValueCodec {
        static constexpr bool enabled = false;
    };

    template<typename T>
    concept HasValueCodec = ValueCodec<T>::enabled;

    /**
     * Enums with names are stored as TEXT. Declare next to the enum (found through ADL):
     *
     *   constexpr auto nearonEnumNames(PaymentMethod) {
     *       return std::array{std::pair{PaymentMethod::Cash, std::string_view("CASH")}, ...};
     *   }
     */
    template<typename E>
    concept NamedEnum = std::is_enum_v<E> && requires { nearonEnumNames(E{}); };

    template<NamedEnum E>
    std::string_view enumName(const E value) {
        for (const auto& [item, name] : nearonEnumNames(E{}))
            if (item == value) return name;
        throw DatabaseError("Enum value without name: " + std::to_string(static_cast<long long>(value)));
    }

    template<NamedEnum E>
    E enumFromName(const std::string_view text) {
        for (const auto& [item, name] : nearonEnumNames(E{}))
            if (name == text) return item;
        throw DatabaseError("Unknown enum name: " + std::string(text));
    }

    /// Built-in mapping between C++ types and SQLite values. Every specialisation exposes:
    /// type, nullable, encode(const T&) -> Value and decode(const Value&) -> T.
    template<typename T>
    struct Codec;

    template<typename T>
    concept Storable = requires(const T& value, const Value& stored) {
        { Codec<T>::type } -> std::convertible_to<SqlType>;
        { Codec<T>::nullable } -> std::convertible_to<bool>;
        { Codec<T>::encode(value) } -> std::same_as<Value>;
        { Codec<T>::decode(stored) } -> std::same_as<T>;
    };

    namespace detail {
        [[noreturn]] inline void nullValue() {
            throw DatabaseError("NULL value read into a non-nullable field");
        }

        inline std::int64_t asInteger(const Value& value) {
            if (const auto* v = std::get_if<std::int64_t>(&value)) return *v;
            if (const auto* v = std::get_if<double>(&value)) return static_cast<std::int64_t>(*v);
            if (const auto* v = std::get_if<std::string>(&value)) return std::stoll(*v);
            if (std::holds_alternative<std::nullptr_t>(value)) nullValue();
            throw DatabaseError("BLOB value read into an integer field");
        }

        inline double asReal(const Value& value) {
            if (const auto* v = std::get_if<double>(&value)) return *v;
            if (const auto* v = std::get_if<std::int64_t>(&value)) return static_cast<double>(*v);
            if (const auto* v = std::get_if<std::string>(&value)) return std::stod(*v);
            if (std::holds_alternative<std::nullptr_t>(value)) nullValue();
            throw DatabaseError("BLOB value read into a real field");
        }

        inline std::string asText(const Value& value) {
            if (const auto* v = std::get_if<std::string>(&value)) return *v;
            if (const auto* v = std::get_if<std::int64_t>(&value)) return std::to_string(*v);
            if (const auto* v = std::get_if<double>(&value)) return std::to_string(*v);
            if (const auto* v = std::get_if<Blob>(&value)) return {v->begin(), v->end()};
            nullValue();
        }

        template<typename T>
        struct IsOptional : std::false_type {};

        template<typename T>
        struct IsOptional<std::optional<T>> : std::true_type {};

        template<typename T>
        struct IsNamedType : std::false_type {};

        template<typename T, typename Tag, template<typename> class... Skills>
        struct IsNamedType<fluent::NamedType<T, Tag, Skills...>> : std::true_type {};
    }

    template<typename T>
    struct UnwrapOptional {
        using type = T;
    };

    template<typename T>
    struct UnwrapOptional<std::optional<T>> {
        using type = T;
    };

    template<typename T>
    using UnwrapOptionalT = typename UnwrapOptional<T>::type;

    template<typename T> requires (std::is_integral_v<T> && !std::is_same_v<T, bool> && !HasValueCodec<T>)
    struct Codec<T> {
        static constexpr SqlType type = SqlType::Integer;
        static constexpr bool nullable = false;

        static Value encode(const T& value) { return static_cast<std::int64_t>(value); }
        static T decode(const Value& value) { return static_cast<T>(detail::asInteger(value)); }
    };

    template<>
    struct Codec<bool> {
        static constexpr SqlType type = SqlType::Integer;
        static constexpr bool nullable = false;

        static Value encode(const bool& value) { return static_cast<std::int64_t>(value ? 1 : 0); }
        static bool decode(const Value& value) { return detail::asInteger(value) != 0; }
    };

    template<typename T> requires (std::is_floating_point_v<T> && !HasValueCodec<T>)
    struct Codec<T> {
        static constexpr SqlType type = SqlType::Real;
        static constexpr bool nullable = false;

        static Value encode(const T& value) { return static_cast<double>(value); }
        static T decode(const Value& value) { return static_cast<T>(detail::asReal(value)); }
    };

    template<typename T> requires (std::is_enum_v<T> && !HasValueCodec<T> && !NamedEnum<T>)
    struct Codec<T> {
        static constexpr SqlType type = SqlType::Integer;
        static constexpr bool nullable = false;

        static Value encode(const T& value) { return static_cast<std::int64_t>(value); }
        static T decode(const Value& value) { return static_cast<T>(detail::asInteger(value)); }
    };

    template<typename T> requires (NamedEnum<T> && !HasValueCodec<T>)
    struct Codec<T> {
        static constexpr SqlType type = SqlType::Text;
        static constexpr bool nullable = false;

        static Value encode(const T& value) { return std::string(enumName(value)); }
        static T decode(const Value& value) { return enumFromName<T>(detail::asText(value)); }
    };

    template<>
    struct Codec<std::string> {
        static constexpr SqlType type = SqlType::Text;
        static constexpr bool nullable = false;

        static Value encode(const std::string& value) { return value; }
        static std::string decode(const Value& value) { return detail::asText(value); }
    };

    template<>
    struct Codec<Blob> {
        static constexpr SqlType type = SqlType::Blob;
        static constexpr bool nullable = false;

        static Value encode(const Blob& value) { return value; }

        static Blob decode(const Value& value) {
            if (const auto* v = std::get_if<Blob>(&value)) return *v;
            const std::string text = detail::asText(value);
            return {text.begin(), text.end()};
        }
    };

    template<typename T> requires Storable<T>
    struct Codec<std::optional<T>> {
        static_assert(!Codec<T>::nullable, "Nested optionals are not supported");

        static constexpr SqlType type = Codec<T>::type;
        static constexpr bool nullable = true;

        static Value encode(const std::optional<T>& value) {
            return value ? Codec<T>::encode(*value) : Value{nullptr};
        }

        static std::optional<T> decode(const Value& value) {
            if (std::holds_alternative<std::nullptr_t>(value)) return std::nullopt;
            return Codec<T>::decode(value);
        }
    };

    /// fluent::NamedType (IDs, strong types) is stored as its underlying type.
    template<typename T, typename Tag, template<typename> class... Skills> requires Storable<T>
    struct Codec<fluent::NamedType<T, Tag, Skills...>> {
        using named_type = fluent::NamedType<T, Tag, Skills...>;

        static constexpr SqlType type = Codec<T>::type;
        static constexpr bool nullable = Codec<T>::nullable;

        static Value encode(const named_type& value) { return Codec<T>::encode(value.get()); }
        static named_type decode(const Value& value) { return named_type(Codec<T>::decode(value)); }
    };

    /// UTC timestamps as "YYYY-MM-DD HH:MM:SS[.ffffff]", the same format SQLite date functions use.
    template<>
    struct Codec<boost::posix_time::ptime> {
        static constexpr SqlType type = SqlType::Text;
        static constexpr bool nullable = false;

        static Value encode(const boost::posix_time::ptime& value) {
            if (value.is_special()) return nullptr;
            std::string text = boost::posix_time::to_iso_extended_string(value);
            if (const auto separator = text.find('T'); separator != std::string::npos) text[separator] = ' ';
            return text;
        }

        static boost::posix_time::ptime decode(const Value& value) {
            if (std::holds_alternative<std::nullptr_t>(value)) return boost::posix_time::not_a_date_time;
            return boost::posix_time::time_from_string(detail::asText(value));
        }
    };

    template<>
    struct Codec<boost::gregorian::date> {
        static constexpr SqlType type = SqlType::Text;
        static constexpr bool nullable = false;

        static Value encode(const boost::gregorian::date& value) {
            if (value.is_special()) return nullptr;
            return boost::gregorian::to_iso_extended_string(value);
        }

        static boost::gregorian::date decode(const Value& value) {
            if (std::holds_alternative<std::nullptr_t>(value)) return boost::gregorian::date(boost::gregorian::not_a_date_time);
            return boost::gregorian::from_simple_string(detail::asText(value));
        }
    };

    template<typename T> requires HasValueCodec<T>
    struct Codec<T> {
        using storage_type = ValueCodec<T>::storage_type;
        using storage_codec = Codec<storage_type>;

        static constexpr SqlType type = storage_codec::type;
        static constexpr bool nullable = storage_codec::nullable;

        static Value encode(const T& value) { return storage_codec::encode(ValueCodec<T>::encode(value)); }
        static T decode(const Value& value) { return ValueCodec<T>::decode(storage_codec::decode(value)); }
    };
}
