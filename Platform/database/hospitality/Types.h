#pragma once

#include "../../NamedType/named_type.hpp"

#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <array>
#include <cstdint>
#include <string_view>
#include <utility>

namespace nexobar {
    template<typename Tag>
    using Id = fluent::NamedType<std::int64_t, Tag, fluent::Comparable, fluent::Hashable, fluent::Printable>;

    using PermissionId = Id<struct PermissionIdTag>;
    using RoleId = Id<struct RoleIdTag>;
    using RolePermissionId = Id<struct RolePermissionIdTag>;
    using UserId = Id<struct UserIdTag>;
    using BusinessId = Id<struct BusinessIdTag>;
    using CategoryId = Id<struct CategoryIdTag>;
    using TaxId = Id<struct TaxIdTag>;
    using ProductId = Id<struct ProductIdTag>;
    using CustomerId = Id<struct CustomerIdTag>;
    using ConsumptionListId = Id<struct ConsumptionListIdTag>;
    using ListLineId = Id<struct ListLineIdTag>;
    using PaymentId = Id<struct PaymentIdTag>;
    using CashSessionId = Id<struct CashSessionIdTag>;
    using CashMovementId = Id<struct CashMovementIdTag>;
    using TicketSeriesId = Id<struct TicketSeriesIdTag>;
    using TicketId = Id<struct TicketIdTag>;
    using TicketLineId = Id<struct TicketLineIdTag>;
    using TicketTaxId = Id<struct TicketTaxIdTag>;
    using FiscalRecordId = Id<struct FiscalRecordIdTag>;
    using FiscalEventId = Id<struct FiscalEventIdTag>;
    using AuditEventId = Id<struct AuditEventIdTag>;

    using Cents = std::int64_t;
    using BasisPoints = std::int32_t;
    using QuantityMilli = std::int64_t;
    using Timestamp = boost::posix_time::ptime;
    using Date = boost::gregorian::date;

    enum class ListStatus { Open, Closed, Cancelled };

    constexpr auto nearonEnumNames(ListStatus) {
        return std::array{
            std::pair{ListStatus::Open, std::string_view("OPEN")},
            std::pair{ListStatus::Closed, std::string_view("CLOSED")},
            std::pair{ListStatus::Cancelled, std::string_view("CANCELLED")},
        };
    }

    enum class PaymentMethod { Cash, Card };

    constexpr auto nearonEnumNames(PaymentMethod) {
        return std::array{
            std::pair{PaymentMethod::Cash, std::string_view("CASH")},
            std::pair{PaymentMethod::Card, std::string_view("CARD")}
        };
    }

    enum class TicketType { Simplified, Complete, RectifyingSimplified, RectifyingComplete };

    constexpr auto nearonEnumNames(TicketType) {
        return std::array{
            std::pair{TicketType::Simplified, std::string_view("SIMPLIFIED")},
            std::pair{TicketType::Complete, std::string_view("COMPLETE")},
            std::pair{TicketType::RectifyingSimplified, std::string_view("RECTIFYING_SIMPLIFIED")},
            std::pair{TicketType::RectifyingComplete, std::string_view("RECTIFYING_COMPLETE")},
        };
    }

    constexpr bool isRectifying(const TicketType type) {
        return type == TicketType::RectifyingSimplified || type == TicketType::RectifyingComplete;
    }

    enum class FiscalRecordType { Issue, Cancellation };

    constexpr auto nearonEnumNames(FiscalRecordType) {
        return std::array{
            std::pair{FiscalRecordType::Issue, std::string_view("ISSUE")},
            std::pair{FiscalRecordType::Cancellation, std::string_view("CANCELLATION")},
        };
    }

    enum class CashMovementDirection { In, Out };

    constexpr auto nearonEnumNames(CashMovementDirection) {
        return std::array{
            std::pair{CashMovementDirection::In, std::string_view("IN")},
            std::pair{CashMovementDirection::Out, std::string_view("OUT")},
        };
    }
}
