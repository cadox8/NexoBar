#pragma once

#include "../../export.h"
#include "../models/Models.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace nexobar {
    using TextProvider = std::function<std::string(std::string_view)>;

    class HospitalityError : public std::runtime_error {
    public:
        explicit HospitalityError(std::string translationKey)
            : std::runtime_error(translationKey), m_translationKey(std::move(translationKey)) {}

        [[nodiscard]] const std::string& translationKey() const noexcept { return m_translationKey; }

    private:
        std::string m_translationKey;
    };

    inline constexpr int SchemaVersion = 4;

    PLATFORM_API void initializeSchema(Nearon::Database& db, const TextProvider& text);
    PLATFORM_API std::shared_ptr<Nearon::Database> openHospitalityDatabase(const std::filesystem::path& path,
                                                                           const TextProvider& text);

    PLATFORM_API void audit(Nearon::Database& db, std::optional<UserId> user, std::string type,
                            std::string entity = {}, std::optional<std::int64_t> entityId = std::nullopt,
                            std::string details = {});

    /// Recomputes the total from a list's current lines.
    PLATFORM_API Cents recalculateList(Nearon::Database& db, ConsumptionListId list);

    /// Adds a fixed-price product-backed line.
    PLATFORM_API ListLine addProductToList(Nearon::Database& db, ConsumptionListId list, ProductId product,
                                           QuantityMilli quantity, UserId user);

    /// Adds a manual line with no product selected, at the entered amount and selected tax rate.
    PLATFORM_API ListLine addManualLine(Nearon::Database& db, ConsumptionListId list, std::string description,
                                        Cents amount, TaxId tax, UserId user, const TextProvider& text);
}
