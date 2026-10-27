#include "database/hospitality/HospitalityDatabase.h"
#include "i18n/I18n.h"
#include "time/AppTime.h"
#include "security/Pin.h"

#include <gtest/gtest.h>

using namespace nexobar;
using Nearon::col;

TEST(HospitalitySchemaTests, RunsScenarios) {
    const platform::I18n i18n(NEXOBAR_TEST_LANGUAGE_DIR, "es-ES");
    const TextProvider text = [&i18n](const std::string_view key) { return i18n.get(key); };
    const auto db = openHospitalityDatabase(":memory:", text);
    initializeSchema(*db, text);
    EXPECT_TRUE(db->userVersion() == SchemaVersion);
    EXPECT_TRUE(db->count<Role>() == 3);
    EXPECT_TRUE(db->count<User>() == 1);
    EXPECT_EQ(db->count<Permission>(), 18);
    EXPECT_TRUE(db->count<Tax>() == 4);
    EXPECT_TRUE(db->count<Category>() == 4);
    EXPECT_EQ(db->count<TicketPlaceholder>(), 25);
    for (const auto& placeholder : db->selectAll<TicketPlaceholder>()) {
        EXPECT_FALSE(placeholder.descriptionKey.empty());
        EXPECT_NE(i18n.get(placeholder.descriptionKey), placeholder.descriptionKey);
    }
    EXPECT_TRUE(db->exists<Category>(CategoryId{0}));

    const User admin = *db->selectFirst<User>(col<&User::displayName> == i18n.get("users.default_admin_name"));
    EXPECT_TRUE(admin.pinHash.empty());
    const auto salt = Pin::newSalt();
    const auto pinHash = Pin::hash("1234", salt);
    EXPECT_TRUE(Pin::valid("1234"));
    EXPECT_FALSE(Pin::valid("12a4"));
    EXPECT_TRUE(Pin::matches("1234", salt, pinHash));
    EXPECT_FALSE(Pin::matches("4321", salt, pinHash));
    EXPECT_EQ(db->count<RolePermission>(col<&RolePermission::roleId> == admin.roleId),
              db->count<Permission>());
    EXPECT_EQ(db->count<Permission>(col<&Permission::key> == "edit_ticket_template"), 1);
    const Tax tax = *db->selectFirst<Tax>(col<&Tax::rate> == 2100);
    const Category category = *db->selectFirst<Category>(col<&Category::name> == i18n.get("categories.drinks"));
    EXPECT_TRUE(db->count<Permission>(col<&Permission::description> == i18n.get("permissions.sell")) == 1);
    EXPECT_TRUE(db->count<Role>(col<&Role::name> == i18n.get("roles.administrator.name")) == 1);
    Product product;
    product.name = "Copa de la casa";
    product.categoryId = category.id;
    product.taxId = tax.id;
    product.price = 650;
    db->insert<Product>(product);
    product = *db->selectFirst<Product>(col<&Product::name> == "Copa de la casa");

    ConsumptionList list;
    list.label = "Grupo cumpleaños";
    list.openedBy = admin.id;
    list.openedAt = AppTime::UTCNow();
    db->insert<ConsumptionList>(list);
    list = *db->selectFirst<ConsumptionList>(col<&ConsumptionList::label> == "Grupo cumpleaños");

    const ListLine productLine = addProductToList(*db, list.id, product.id, 2000, admin.id);
    const ListLine manualLine = addManualLine(*db, list.id, "Ronda especial", 1200, tax.id, admin.id, text);
    EXPECT_TRUE(productLine.productId == product.id);
    EXPECT_TRUE(!manualLine.productId);
    EXPECT_TRUE(recalculateList(*db, list.id) == 2500);
    EXPECT_TRUE(db->count<ListLine>(col<&ListLine::listId> == list.id) == 2);

    // Schema initialization normalizes both discontinued legacy options.
    auto legacyProduct = product;
    legacyProduct.openPrice = true;
    db->update(legacyProduct);
    auto legacyCategory = category;
    const auto otherCategory = db->query<Category>()
        .where(col<&Category::id> != category.id).first();
    ASSERT_TRUE(otherCategory.has_value());
    legacyCategory.parentId = otherCategory->id;
    db->update(legacyCategory);
    Business existingBusiness;
    existingBusiness.legalName = "Negocio existente S.L.";
    existingBusiness.tradeName = "Bar existente";
    existingBusiness.nif = "B12345678";
    existingBusiness.address = "Calle conservada 1";
    existingBusiness.postalCode = "28001";
    existingBusiness.city = "Madrid";
    existingBusiness.province = "Madrid";
    existingBusiness.country = "ES";
    existingBusiness.phone = "910000000";
    existingBusiness.email = "bar@example.test";
    existingBusiness.license = "LICENCIA-EXISTENTE";
    db->insert(existingBusiness);
    initializeSchema(*db, text);
    EXPECT_FALSE(db->get<Product>(product.id).openPrice);
    EXPECT_FALSE(db->get<Category>(category.id).parentId.has_value());
    const auto preservedBusiness = db->get<Business>(existingBusiness.id);
    EXPECT_EQ(preservedBusiness.tradeName, existingBusiness.tradeName);
    EXPECT_EQ(preservedBusiness.legalName, existingBusiness.legalName);
    EXPECT_EQ(preservedBusiness.nif, existingBusiness.nif);
    EXPECT_EQ(preservedBusiness.address, existingBusiness.address);
    EXPECT_EQ(preservedBusiness.license, existingBusiness.license);

    EXPECT_TRUE(db->scalar<std::int64_t>("SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name IN ('dining_tables','orders','order_lines')") == 0);
    EXPECT_TRUE(db->scalar<std::int64_t>("SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name IN ('lists','tickets','ticket_lines','fiscal_records','fiscal_events')") == 5);
    EXPECT_TRUE(db->scalar<std::int64_t>("SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='ticket_placeholders'") == 1);

}
