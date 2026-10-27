#include "ProductState.h"

#include "../../../NexoBar.h"
#include "database/hospitality/HospitalityDatabase.h"

#include <algorithm>

ProductState::ProductState(Nearon::Database* database, const std::int64_t selectedUserId) : m_database(database) {
    if (database) {
        if (const auto business = database->query<nexobar::Business>().first()) {
            businessName = business->tradeName.empty()
                               ? business->legalName
                               : business->tradeName;
            businessTaxNumber = business->nif;
            businessAddress = business->address;
        }
        const auto users = database->query<nexobar::User>()
                .where(Nearon::col < &nexobar::User::active > == true)
                .orderBy(Nearon::col < &nexobar::User::id >.asc())
                .all();
        const auto selected = selectedUserId > 0
            ? std::ranges::find(users, nexobar::UserId{selectedUserId}, &nexobar::User::id)
            : users.begin();
        if (selected != users.end()) {
            userId = selected->id.get();
            roleId = selected->roleId.get();
            userName = selected->displayName;
        }
        reloadCatalog();
    }
    quantities.resize(products.size(), 0);
    NexoBarImp nexo;
    if (businessName.empty()) businessName = nexo->text("header.business_fallback");
    if (userName.empty()) userName = nexo->text("users.default_admin_name");
}

bool ProductState::hasPermission(const std::string& permissionKey) const {
    if (!m_database || roleId <= 0)
        return false;
    const auto permission = m_database->selectFirst<nexobar::Permission>(
        Nearon::col < &nexobar::Permission::key > == permissionKey &&
        Nearon::col < &nexobar::Permission::active > == true);
    if (!permission)
        return false;
    return m_database->count<nexobar::RolePermission>(
               Nearon::col < &nexobar::RolePermission::roleId > ==
               nexobar::RoleId{roleId} &&
               Nearon::col < &nexobar::RolePermission::permissionId > ==
               permission->id) > 0;
}

void ProductState::refreshSessionIdentity() {
    if (!m_database || userId <= 0)
        return;
    const auto user = m_database->select<nexobar::User>(nexobar::UserId{userId});
    if (!user)
        return;
    userName = user->displayName;
    roleId = user->roleId.get();
}

bool ProductState::canOpenSettings() const {
    static constexpr const char* managementPermissions[] = {
        "manage_users", "manage_roles", "manage_permissions", "manage_cash",
        "view_reports", "configure_business", "configure_printer", "edit_ticket_template"
    };
    return std::ranges::any_of(managementPermissions, [this](const char* key) {
        return hasPermission(key);
    });
}

void ProductState::reloadCatalog() {
    if (!m_database)
        return;
    std::vector<std::pair<Product, int> > retained;
    for (const auto index: selectedOrder)
        if (index < products.size() && index < quantities.size() &&
            quantities[index] > 0)
            retained.emplace_back(products[index], quantities[index]);
    categories.clear();
    products.clear();
    taxes.clear();
    for (const auto& c:
         m_database->query<nexobar::Category>()
         .where(Nearon::col < &nexobar::Category::active > == true)
         .orderBy(Nearon::col < &nexobar::Category::id >.asc())
         .all())
        categories.push_back({c.id.get(), c.name, c.icon});
    for (const auto& p:
         m_database->query<nexobar::Product>()
         .where(Nearon::col < &nexobar::Product::active > == true)
         .orderBy(Nearon::col < &nexobar::Product::id >.asc())
         .all())
        products.push_back({p.id.get(), p.categoryId.get(), p.name, p.price,
                            p.taxId.get()});
    for (const auto& t: m_database->query<nexobar::Tax>()
         .where(Nearon::col < &nexobar::Tax::active > == true)
         .orderBy(Nearon::col < &nexobar::Tax::id >.asc())
         .all())
        taxes.push_back({t.id.get(), t.name, t.rate});
    quantities.assign(products.size(), 0);
    selectedOrder.clear();
    for (const auto& [oldProduct, count]: retained) {
        const auto it = std::ranges::find(products, oldProduct.id, &Product::id);
        if (it == products.end()) {
            if (oldProduct.id >= 0)
                continue;
            products.push_back(oldProduct);
            quantities.push_back(0);
        }
        const auto target =
                std::ranges::find(products, oldProduct.id, &Product::id);
        const auto index = static_cast<std::size_t>(target - products.begin());
        quantities[index] = count;
        selectedOrder.push_back(index);
    }
}

bool ProductState::createCategory(const std::string& name,
                                  const std::string& icon) {
    if (!m_database || name.empty())
        return false;
    nexobar::Category row;
    row.name = name;
    row.icon = icon.empty() ? "logo.png" : icon;
    row.parentId.reset();
    m_database->insert(row);
    reloadCatalog();
    return true;
}

bool ProductState::createProduct(const std::string& name,
                                 std::int64_t categoryId, std::int64_t taxId,
                                 std::int64_t priceMinor) {
    if (!m_database || name.empty() || categoryId <= 0 || taxId <= 0 ||
        priceMinor < 0)
        return false;
    nexobar::Product row;
    row.name = name;
    row.categoryId = nexobar::CategoryId{categoryId};
    row.taxId = nexobar::TaxId{taxId};
    row.price = priceMinor;
    row.openPrice = false;
    m_database->insert(row);
    reloadCatalog();
    return true;
}

bool ProductState::updateCategory(std::int64_t id, const std::string& name,
                                  const std::string& icon) {
    if (!m_database || name.empty())
        return false;
    auto row = m_database->select<nexobar::Category>(nexobar::CategoryId{id});
    if (!row || !row->active)
        return false;
    row->name = name;
    row->icon = icon.empty() ? "logo.png" : icon;
    row->parentId.reset();
    if (!m_database->update(*row))
        return false;
    reloadCatalog();
    return true;
}

bool ProductState::updateProduct(std::int64_t id, const std::string& name,
                                 std::int64_t categoryId, std::int64_t taxId,
                                 std::int64_t priceMinor) {
    if (!m_database || name.empty() || categoryId <= 0 || taxId <= 0 ||
        priceMinor < 0)
        return false;
    auto row = m_database->select<nexobar::Product>(nexobar::ProductId{id});
    if (!row || !row->active)
        return false;
    row->name = name;
    row->categoryId = nexobar::CategoryId{categoryId};
    row->taxId = nexobar::TaxId{taxId};
    row->price = priceMinor;
    row->openPrice = false;
    if (!m_database->update(*row))
        return false;
    reloadCatalog();
    orderChanged();
    return true;
}

bool ProductState::deactivateProduct(std::int64_t id) {
    if (!m_database)
        return false;
    auto row = m_database->select<nexobar::Product>(nexobar::ProductId{id});
    if (!row || !row->active)
        return false;
    row->active = false;
    m_database->update(*row);
    reloadCatalog();
    orderChanged();
    return true;
}

bool ProductState::deactivateCategory(std::int64_t id) {
    if (!m_database)
        return false;
    auto row = m_database->select<nexobar::Category>(nexobar::CategoryId{id});
    if (!row || !row->active)
        return false;
    m_database->transaction([&] {
        row->active = false;
        m_database->update(*row);
        auto products = m_database->query<nexobar::Product>()
                .where(Nearon::col < &nexobar::Product::categoryId > ==
                       nexobar::CategoryId{id})
                .where(Nearon::col < &nexobar::Product::active > == true)
                .all();
        for (auto& product: products) {
            product.active = false;
            m_database->update(product);
        }
    });
    reloadCatalog();
    orderChanged();
    return true;
}

void ProductState::addProduct(const std::size_t index) {
    if (index >= products.size() || index >= quantities.size())
        return;
    if (quantities[index] == 0)
        selectedOrder.push_back(index);
    ++quantities[index];
    orderChanged();
}

void ProductState::removeProduct(const std::size_t index) {
    if (index >= quantities.size() || quantities[index] <= 0)
        return;
    quantities[index] = 0;
    std::erase(selectedOrder, index);
    orderChanged();
}

void ProductState::clearOrder() {
    std::fill(quantities.begin(), quantities.end(), 0);
    selectedOrder.clear();
    orderChanged();
}

std::int64_t ProductState::total() const {
    std::int64_t amount = 0;
    for (std::size_t index = 0;
         index < products.size() && index < quantities.size(); ++index)
        amount += products[index].priceMinor * quantities[index];
    return amount;
}

std::string ProductState::formatPrice(const std::int64_t amountMinor) const {
    const auto whole = amountMinor / 100;
    const auto cents = amountMinor % 100;
    NexoBarImp nexo;
    const std::string separator = nexo->text("number.decimal_separator");
    const std::string currency = nexo->text("number.currency_suffix");
    return std::to_string(whole) + separator + (cents < 10 ? "0" : "") +
           std::to_string(cents) + currency;
}
