#include "database/hospitality/HospitalityDatabase.h"
#include "time/AppTime.h"

#include <gtest/gtest.h>

using namespace nexobar;
using Nearon::col;

TEST(HospitalitySchemaTests, RunsScenarios) {
    const platform::I18n i18n(NEXOBAR_TEST_LANGUAGE_DIR, "es-ES");
    const auto db = openHospitalityDatabase(":memory:", i18n);
    initializeSchema(*db, i18n);
    EXPECT_TRUE(db->userVersion() == SchemaVersion);
    EXPECT_TRUE(db->count<Role>() == 3);
    EXPECT_TRUE(db->count<User>() == 1);
    EXPECT_TRUE(db->count<Permission>() >= 10);
    EXPECT_TRUE(db->count<Tax>() == 4);
    EXPECT_TRUE(db->count<Category>() == 4);
    EXPECT_TRUE(db->exists<Category>(CategoryId{0}));

    const User admin = *db->selectFirst<User>(col<&User::displayName> == i18n.get("users.default_admin_name"));
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
    const ListLine manualLine = addManualLine(*db, list.id, "Ronda especial", 1200, tax.id, admin.id, i18n);
    EXPECT_TRUE(productLine.productId == product.id);
    EXPECT_TRUE(!manualLine.productId);
    EXPECT_TRUE(recalculateList(*db, list.id) == 2500);
    EXPECT_TRUE(db->count<ListLine>(col<&ListLine::listId> == list.id) == 2);

    Product openPrice;
    openPrice.name = "Copa premium (precio manual)";
    openPrice.categoryId = category.id;
    openPrice.taxId = tax.id;
    openPrice.openPrice = true;
    db->insert<Product>(openPrice);
    openPrice = *db->selectFirst<Product>(col<&Product::name> == "Copa premium (precio manual)");
    bool rejected = false;
    try { addProductToList(*db, list.id, openPrice.id, 1000, admin.id); }
    catch (const HospitalityError&) { rejected = true; }
    EXPECT_TRUE(rejected);
    EXPECT_TRUE(addProductToList(*db, list.id, openPrice.id, 1000, admin.id, 900).unitPrice == 900);

    EXPECT_TRUE(db->scalar<std::int64_t>("SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name IN ('dining_tables','orders','order_lines')") == 0);
    EXPECT_TRUE(db->scalar<std::int64_t>("SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name IN ('lists','tickets','ticket_lines','fiscal_records','fiscal_events')") == 5);

}
