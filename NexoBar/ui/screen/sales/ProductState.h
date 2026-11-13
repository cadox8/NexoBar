#pragma once

#include "../../../manager/AssetManager.h"

#include <boost/signals2.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Nearon {
    class Database;
}

struct ProductState {
    struct Category {
        std::int64_t id;
        std::string name;
        std::string icon = "logo.png";
    };

    struct Product {
        std::int64_t id;
        std::int64_t categoryId;
        std::string name;
        std::int64_t priceMinor;
        std::int64_t taxId = 0;
    };

    struct Tax {
        std::int64_t id;
        std::string name;
        std::int64_t rate;
        std::string taxCode;
        std::string regimeCode;
        std::string operationCode;
    };

    explicit ProductState(Nearon::Database* database, std::int64_t selectedUserId = 0);

    void reloadCatalog();

    // Refreshes mutable session and business data without replacing the current
    // sale or its selected products.
    void refreshSessionIdentity();

    bool createCategory(const std::string& name, const std::string& icon);

    bool createProduct(const std::string& name, std::int64_t categoryId,
                       std::int64_t taxId, std::int64_t priceMinor);

    bool updateCategory(std::int64_t id, const std::string& name,
                        const std::string& icon);

    bool updateProduct(std::int64_t id, const std::string& name,
                       std::int64_t categoryId, std::int64_t taxId,
                       std::int64_t priceMinor);

    bool deactivateCategory(std::int64_t id);

    bool deactivateProduct(std::int64_t id);

    void addProduct(std::size_t index);

    void removeProduct(std::size_t index);

    void clearOrder();

    std::int64_t total() const;

    std::string formatPrice(std::int64_t amountMinor) const;

    [[nodiscard]] bool hasPermission(const std::string& permissionKey) const;

    [[nodiscard]] bool canOpenSettings() const;

    boost::signals2::signal<void()> orderChanged;

    std::vector<Category> categories;
    std::vector<Product> products;
    std::vector<Tax> taxes;
    std::vector<int> quantities;
    std::vector<std::size_t> selectedOrder;
    std::int64_t userId = 0;
    std::int64_t roleId = 0;
    std::string businessName;
    std::string userName;
    std::string businessTaxNumber;
    std::string businessAddress;
    TexturePtr fallbackImage;

private:
    Nearon::Database* m_database = nullptr;
};
