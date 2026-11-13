//
// Created by cadox8 on 23/10/2026.
//
#pragma once

#include "Types.h"
#include "../../i18n/I18n.h"

#include <algorithm>
#include <string>
#include <vector>

namespace nexobar {
    /// Integer division rounding half away from zero (the usual rounding for amounts).
    constexpr std::int64_t roundDiv(const std::int64_t numerator, const std::int64_t denominator) {
        const bool negative = (numerator < 0) != (denominator < 0);
        const std::int64_t n = numerator < 0 ? -numerator : numerator;
        const std::int64_t d = denominator < 0 ? -denominator : denominator;
        const std::int64_t result = (n + d / 2) / d;
        return negative ? -result : result;
    }

    /// Line total (VAT included) for a unit price and a quantity in thousandths.
    constexpr Cents lineTotal(const Cents unitPrice, const QuantityMilli quantity, const Cents discount = 0) {
        return roundDiv(unitPrice * quantity, 1000) - discount;
    }

    /// Tax base contained in an amount with VAT (and surcharge) included.
    constexpr Cents baseFromGross(const Cents gross, const BasisPoints rate, const BasisPoints surcharge = 0) {
        return roundDiv(gross * 10000, 10000 + rate + surcharge);
    }

    struct TaxBreakdown {
        BasisPoints rate{};
        BasisPoints surchargeRate{};
        Cents base{};
        Cents tax{};
        Cents surcharge{};
        /// base + tax + surcharge, VAT included.
        Cents total{};
    };

    /**
     * Groups VAT-included line totals per rate and splits each group into base and tax. Rounding is applied once
     * per rate, so base + tax always equals the charged total.
     * Each element of lines must expose total, taxRate and surchargeRate (OrderLine, InvoiceLine).
     */
    template<typename Lines>
    std::vector<TaxBreakdown> taxBreakdown(const Lines& lines) {
        std::vector<TaxBreakdown> result;
        for (const auto& line: lines) {
            auto it = std::ranges::find_if(result, [&](const TaxBreakdown& item) {
                return item.rate == line.taxRate && item.surchargeRate == line.surchargeRate;
            });
            if (it == result.end()) it = result.insert(result.end(), TaxBreakdown{line.taxRate, line.surchargeRate});
            it->total += line.total;
        }
        for (TaxBreakdown& item: result) {
            item.base = baseFromGross(item.total, item.rate, item.surchargeRate);
            item.surcharge = roundDiv(item.base * item.surchargeRate, 10000);
            item.tax = item.total - item.base - item.surcharge;
        }
        std::ranges::sort(result, {}, &TaxBreakdown::rate);
        return result;
    }

    /// "1.234,50 €"
    inline std::string formatMoney(const Cents cents, const bool withSymbol = true) {
        const bool negative = cents < 0;
        const std::int64_t absolute = negative ? -cents : cents;
        std::string euros = std::to_string(absolute / 100);
        for (auto i = static_cast<std::ptrdiff_t>(euros.size()) - 3; i > 0; i -= 3)
            euros.insert(static_cast<std::size_t>(i), ".");
        const std::int64_t remainder = absolute % 100;
        std::string result = (negative ? "-" : "") + euros + "," + (remainder < 10 ? "0" : "") + std::to_string(remainder);
        return withSymbol ? result + " €" : result;
    }

    inline std::string formatMoney(const Cents cents, const platform::I18n& i18n, const bool withSymbol = true) {
        const bool negative = cents < 0;
        const std::int64_t absolute = negative ? -cents : cents;
        std::string euros = std::to_string(absolute / 100);
        const std::string group = i18n.get("number.group_separator");
        for (auto i = static_cast<std::ptrdiff_t>(euros.size()) - 3; i > 0; i -= 3)
            euros.insert(static_cast<std::size_t>(i), group);
        const std::int64_t remainder = absolute % 100;
        std::string result = (negative ? "-" : "") + euros + i18n.get("number.decimal_separator")
                             + (remainder < 10 ? "0" : "") + std::to_string(remainder);
        return withSymbol ? result + i18n.get("number.currency_suffix") : result;
    }

    /// "10 %", "5,5 %"
    inline std::string formatRate(const BasisPoints rate) {
        std::string result = std::to_string(rate / 100);
        if (const BasisPoints decimals = rate % 100; decimals != 0) {
            result += "," + std::string(decimals < 10 ? "0" : "") + std::to_string(decimals);
            if (result.back() == '0') result.pop_back();
        }
        return result + "%";
    }
}
