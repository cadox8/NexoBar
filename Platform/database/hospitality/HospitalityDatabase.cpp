#include "HospitalityDatabase.h"
#include "Money.h"

#include "../../time/AppTime.h"

#include <algorithm>
#include <string_view>

namespace nexobar {
    using Nearon::col;

    namespace {
        Timestamp now() { return AppTime::UTCNow(); }

        void createImmutableTriggers(Nearon::Database& db) {
            constexpr std::string_view immutableTables[] = {
                "tickets", "ticket_lines", "ticket_taxes", "fiscal_records", "fiscal_events", "audit_events"
            };
            for (const std::string_view table: immutableTables) {
                for (const std::string_view operation: {"UPDATE", "DELETE"}) {
                    const std::string trigger = std::string(table) + "_no_" + std::string(operation);
                    db.execute("DROP TRIGGER IF EXISTS \"" + trigger + "\"");
                    db.execute("CREATE TRIGGER IF NOT EXISTS \"" + trigger + "\" BEFORE " + std::string(operation)
                               + " ON \"" + std::string(table)
                               + "\" BEGIN SELECT RAISE(ABORT, 'errors.historical_immutable'); END");
                }
            }
        }

        void seedPermissionsAndRoles(Nearon::Database& db, const platform::I18n& i18n) {
            if (db.count<Permission>() == 0) {
                const std::pair<const char*, const char*> permissions[] = {
                    {"sell", "permissions.sell"},
                    {"discount", "permissions.discount"},
                    {"remove_line", "permissions.remove_line"},
                    {"close_list", "permissions.close_list"},
                    {"void_ticket", "permissions.void_ticket"},
                    {"manage_products", "permissions.manage_products"},
                    {"manage_users", "permissions.manage_users"},
                    {"manage_cash", "permissions.manage_cash"},
                    {"view_reports", "permissions.view_reports"},
                    {"configure_business", "permissions.configure_business"},
                    {"export_fiscal", "permissions.export_fiscal"},
                };
                for (const auto& [key, descriptionKey]: permissions)
                    db.insert<Permission>(Permission{PermissionId{0}, key, i18n.get(descriptionKey), true});
            }

            if (db.count<Role>() == 0) {
                db.insert<Role>(Role{.id = RoleId{0}, .name = i18n.get("roles.administrator.name"), .description = i18n.get("roles.administrator.description"), .active = true});
                db.insert<Role>(Role{.id = RoleId{0}, .name = i18n.get("roles.manager.name"), .description = i18n.get("roles.manager.description"), .active = true});
                db.insert<Role>(Role{.id = RoleId{0}, .name = i18n.get("roles.bartender.name"), .description = i18n.get("roles.bartender.description"), .active = true});

                const Role administrator = db.get<Role>(RoleId{1});
                const Role manager = *db.selectFirst<Role>(col<&Role::name> == i18n.get("roles.manager.name"));
                const Role bartender = *db.selectFirst<Role>(col<&Role::name> == i18n.get("roles.bartender.name"));
                for (const Permission& permission: db.selectAll<Permission>()) {
                    const auto grant = [&](const Role& role) {
                        db.insert<RolePermission>(RolePermission{RolePermissionId{0}, role.id, permission.id});
                    };
                    grant(administrator);
                    if (permission.key != "manage_users" && permission.key != "configure_business") grant(manager);
                    if (permission.key == "sell" || permission.key == "close_list") grant(bartender);
                }
            }

            if (db.count<User>() == 0) {
                const auto adminRole = db.selectFirst<Role>(col<&Role::name> == i18n.get("roles.administrator.name"));
                if (!adminRole) throw HospitalityError("errors.administrator_role_missing");
                db.insert<User>(User{UserId{0}, i18n.get("users.default_admin_name"), adminRole->id, true, now()});
            }
        }

        void seedCatalogue(Nearon::Database& db, const platform::I18n& i18n) {
            if (db.count<Tax>() == 0) {
                db.insert<Tax>(Tax{TaxId{0}, i18n.get("taxes.general_21"), 2100, true});
                db.insert<Tax>(Tax{TaxId{0}, i18n.get("taxes.reduced_10"), 1000, true});
                db.insert<Tax>(Tax{TaxId{0}, i18n.get("taxes.super_reduced_4"), 400, true});
                db.insert<Tax>(Tax{TaxId{0}, i18n.get("taxes.exempt_0"), 0, true});
            }
            if (db.count<Category>() == 0) {
                db.insert<Category>(Category{.id = CategoryId{0}, .name = i18n.get("categories.drinks"), .parentId = std::nullopt, .icon = "drinks.png", .active = true});
                db.insert<Category>(Category{.id = CategoryId{0}, .name = i18n.get("categories.coffee"), .parentId = std::nullopt, .icon = "coffee.png", .active = true});
                db.insert<Category>(Category{.id = CategoryId{0}, .name = i18n.get("categories.other"), .parentId = std::nullopt, .icon = "other.png", .active = true});
            }
            db.run("INSERT OR IGNORE INTO categories (id, name, parent_id, icon, active) "
                   "VALUES (0, ?, NULL, ?, 1)", {i18n.get("categories.default"), std::string("default")});
        }
    }

    void initializeSchema(Nearon::Database& db, const platform::I18n& i18n) {
        db.transaction([&] {
            db.createTables<Permission, Role, RolePermission, User, Business, Category, Tax, Product, Customer,
                ConsumptionList, ListLine, CashSession, CashMovement, Payment, TicketSeries, Ticket,
                TicketLine, TicketTax, FiscalRecord, FiscalEvent, AuditEvent>();
            createImmutableTriggers(db);
            seedPermissionsAndRoles(db, i18n);
            seedCatalogue(db, i18n);
            db.setUserVersion(SchemaVersion);
        });
    }

    std::shared_ptr<Nearon::Database> openHospitalityDatabase(const std::filesystem::path& path, const platform::I18n& i18n) {
        if (path != ":memory:" && path.has_parent_path())
            std::filesystem::create_directories(path.parent_path());
        auto db = std::make_shared<Nearon::Database>(path);
        initializeSchema(*db, i18n);
        return db;
    }

    void audit(Nearon::Database& db, const std::optional<UserId> user, std::string type, std::string entity,
               const std::optional<std::int64_t> entityId, std::string details) {
        db.insert<AuditEvent>(AuditEvent{
            AuditEventId{0}, now(), user, std::move(type), std::move(entity), entityId,
            std::move(details)
        });
    }

    Cents recalculateList(Nearon::Database& db, const ConsumptionListId listId) {
        const Cents total = db.sum<&ListLine::total>(col<&ListLine::listId> == listId);
        auto list = db.get<ConsumptionList>(listId);
        list.total = total;
        db.update<ConsumptionList>(list);
        return total;
    }

    ListLine addProductToList(Nearon::Database& db, const ConsumptionListId listId, const ProductId productId,
                              const QuantityMilli quantity, const UserId user, const std::optional<Cents> openPrice) {
        if (quantity <= 0) throw HospitalityError("errors.quantity_positive");
        return db.transaction([&] {
            const auto list = db.get<ConsumptionList>(listId);
            if (list.status != ListStatus::Open) throw HospitalityError("errors.list_not_open");
            const auto product = db.get<Product>(productId);
            if (!product.active) throw HospitalityError("errors.product_inactive");
            if (product.openPrice && !openPrice) throw HospitalityError("errors.open_price_required");
            if (!product.openPrice && openPrice) throw HospitalityError("errors.fixed_price_override");
            const Tax tax = db.get<Tax>(product.taxId);
            if (!tax.active) throw HospitalityError("errors.product_tax_inactive");

            const Cents unitPrice = openPrice.value_or(product.price);
            if (unitPrice < 0) throw HospitalityError("errors.price_negative");
            ListLine line{
                ListLineId{0}, listId, productId, product.name, quantity, unitPrice, 0,
                tax.id, tax.rate, lineTotal(unitPrice, quantity), user, now()
            };
            db.insert<ListLine>(line);
            recalculateList(db, listId);
            audit(db, user, "LIST_LINE_ADDED", "list_lines", line.id.get(), product.name);
            return line;
        });
    }

    ListLine addManualLine(Nearon::Database& db, const ConsumptionListId listId, std::string description,
                           const Cents amount, const TaxId taxId, const UserId user, const platform::I18n& i18n) {
        if (description.empty()) description = i18n.get("lists.manual_amount");
        if (amount <= 0) throw HospitalityError("errors.manual_amount_positive");
        return db.transaction([&] {
            const ConsumptionList list = db.get<ConsumptionList>(listId);
            if (list.status != ListStatus::Open) throw HospitalityError("errors.list_not_open");
            const Tax tax = db.get<Tax>(taxId);
            if (!tax.active) throw HospitalityError("errors.selected_tax_inactive");
            ListLine line{
                ListLineId{0}, listId, std::nullopt, std::move(description), 1000, amount, 0,
                tax.id, tax.rate, amount, user, now()
            };
            db.insert<ListLine>(line);
            recalculateList(db, listId);
            audit(db, user, "MANUAL_LINE_ADDED", "list_lines", line.id.get(), line.description);
            return line;
        });
    }
}
