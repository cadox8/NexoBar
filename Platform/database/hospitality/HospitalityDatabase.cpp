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

        void seedPermissionsAndRoles(Nearon::Database& db, const TextProvider& text) {
            {
                const std::pair<const char*, const char*> permissions[] = {
                    {"sell", "permissions.sell"},
                    {"discount", "permissions.discount"},
                    {"remove_line", "permissions.remove_line"},
                    {"close_list", "permissions.close_list"},
                    {"print_ticket", "permissions.print_ticket"},
                    {"open_drawer", "permissions.open_drawer"},
                    {"void_ticket", "permissions.void_ticket"},
                    {"manage_products", "permissions.manage_products"},
                    {"manage_users", "permissions.manage_users"},
                    {"manage_roles", "permissions.manage_roles"},
                    {"manage_permissions", "permissions.manage_permissions"},
                    {"manage_cash", "permissions.manage_cash"},
                    {"view_reports", "permissions.view_reports"},
                    {"print_reports", "permissions.print_reports"},
                    {"configure_business", "permissions.configure_business"},
                    {"configure_printer", "permissions.configure_printer"},
                    {"edit_ticket_template", "permissions.edit_ticket_template"},
                    {"export_fiscal", "permissions.export_fiscal"},
                };
                for (const auto& [key, descriptionKey]: permissions) {
                    auto existing = db.selectFirst<Permission>(col<&Permission::key> == key);
                    if (existing) {
                        existing->description = text(descriptionKey);
                        existing->active = true;
                        db.update(*existing);
                    } else db.insert<Permission>(Permission{PermissionId{0}, key, text(descriptionKey), true});
                }
            }

            if (db.count<Role>() == 0) {
                db.insert<Role>(Role{.id = RoleId{0}, .name = text("roles.administrator.name"), .description = text("roles.administrator.description"), .active = true});
                db.insert<Role>(Role{.id = RoleId{0}, .name = text("roles.manager.name"), .description = text("roles.manager.description"), .active = true});
                db.insert<Role>(Role{.id = RoleId{0}, .name = text("roles.bartender.name"), .description = text("roles.bartender.description"), .active = true});

            }

            const auto seededRoles = db.query<Role>().orderBy(col<&Role::id>.asc()).limit(3).all();
            if (!seededRoles.empty()) {
                const auto grant = [&](const Role& role, const Permission& permission) {
                    const auto condition = col<&RolePermission::roleId> == role.id &&
                                           col<&RolePermission::permissionId> == permission.id;
                    if (db.count<RolePermission>(condition) == 0)
                        db.insert<RolePermission>(RolePermission{RolePermissionId{0}, role.id, permission.id});
                };
                for (const Permission& permission : db.selectAll<Permission>()) {
                    grant(seededRoles[0], permission);
                    if (seededRoles.size() > 1 && permission.key != "manage_users" &&
                        permission.key != "manage_roles" && permission.key != "manage_permissions" &&
                        permission.key != "configure_business" && permission.key != "export_fiscal")
                        grant(seededRoles[1], permission);
                    if (seededRoles.size() > 2 && (permission.key == "sell" || permission.key == "close_list" ||
                        permission.key == "print_ticket" || permission.key == "open_drawer"))
                        grant(seededRoles[2], permission);
                }
            }

            if (db.count<User>() == 0) {
                const auto adminRole = db.selectFirst<Role>(col<&Role::name> == text("roles.administrator.name"));
                if (!adminRole) throw HospitalityError("errors.administrator_role_missing");
                db.insert<User>(User{UserId{0}, text("users.default_admin_name"), adminRole->id, true, now()});
            }
        }

        void seedCatalogue(Nearon::Database& db, const TextProvider& text) {
            if (db.count<Tax>() == 0) {
                db.insert<Tax>(Tax{TaxId{0}, text("taxes.general_21"), 2100, true});
                db.insert<Tax>(Tax{TaxId{0}, text("taxes.reduced_10"), 1000, true});
                db.insert<Tax>(Tax{TaxId{0}, text("taxes.super_reduced_4"), 400, true});
                db.insert<Tax>(Tax{TaxId{0}, text("taxes.exempt_0"), 0, true});
            }
            if (db.count<Category>() == 0) {
                db.insert<Category>(Category{.id = CategoryId{0}, .name = text("categories.drinks"), .parentId = std::nullopt, .icon = "drinks.png", .active = true});
                db.insert<Category>(Category{.id = CategoryId{0}, .name = text("categories.coffee"), .parentId = std::nullopt, .icon = "coffee.png", .active = true});
                db.insert<Category>(Category{.id = CategoryId{0}, .name = text("categories.other"), .parentId = std::nullopt, .icon = "other.png", .active = true});
            }
            db.run("INSERT OR IGNORE INTO categories (id, name, parent_id, icon, active) "
                   "VALUES (0, ?, NULL, ?, 1)", {text("categories.default"), std::string("default")});
        }

        void seedTicketPlaceholders(Nearon::Database& db) {
            struct Seed {
                const char* placeholder;
                const char* descriptionKey;
                const char* source;
            };
            constexpr Seed seeds[] = {
                {"{{business.name}}", "ticket_placeholders.business_name", "{{business.name}}\n"},
                {"{{business.legal_name}}", "ticket_placeholders.business_legal_name", "{{business.legal_name}}\n"},
                {"{{business.tax_id}}", "ticket_placeholders.business_tax_id", "{{business.tax_id}}\n"},
                {"{{business.address}}", "ticket_placeholders.business_address", "{{business.address}}\n"},
                {"{{business.postal_code}}", "ticket_placeholders.business_postal_code", "{{business.postal_code}}\n"},
                {"{{business.city}}", "ticket_placeholders.business_city", "{{business.city}}\n"},
                {"{{business.province}}", "ticket_placeholders.business_province", "{{business.province}}\n"},
                {"{{business.country}}", "ticket_placeholders.business_country", "{{business.country}}\n"},
                {"{{business.phone}}", "ticket_placeholders.business_phone", "{{business.phone}}\n"},
                {"{{business.email}}", "ticket_placeholders.business_email", "{{business.email}}\n"},
                {"{{ticket.number}}", "ticket_placeholders.ticket_number", "{{ticket.number}}\n"},
                {"{{ticket.date}}", "ticket_placeholders.ticket_date", "{{ticket.date}}\n"},
                {"{{ticket.total}}", "ticket_placeholders.ticket_total", "{{ticket.total}}\n"},
                {"{{ticket.tax_base}}", "ticket_placeholders.ticket_tax_base", "{{ticket.tax_base}}\n"},
                {"{{ticket.tax_amount}}", "ticket_placeholders.ticket_tax_amount", "{{ticket.tax_amount}}\n"},
                {"{{ticket.currency}}", "ticket_placeholders.ticket_currency", "{{ticket.currency}}\n"},
                {"{{ticket.type}}", "ticket_placeholders.ticket_type", "{{ticket.type}}\n"},
                {"{{ticket.footer}}", "ticket_placeholders.ticket_footer", "{{ticket.footer}}\n"},
                {"{{separator}}", "ticket_placeholders.separator", "{{separator}}\n"},
                {"{{line.row}}", "ticket_placeholders.line_row", "{{#each lines}}{{line.row}}\n{{/each}}"},
                {"{{line.description}}", "ticket_placeholders.line_description", "{{#each lines}}{{line.description}}\n{{/each}}"},
                {"{{line.quantity}}", "ticket_placeholders.line_quantity", "{{#each lines}}{{line.quantity}}\n{{/each}}"},
                {"{{line.unit_price}}", "ticket_placeholders.line_unit_price", "{{#each lines}}{{line.unit_price}}\n{{/each}}"},
                {"{{line.tax_rate}}", "ticket_placeholders.line_tax_rate", "{{#each lines}}{{line.tax_rate}}\n{{/each}}"},
                {"{{line.total}}", "ticket_placeholders.line_total", "{{#each lines}}{{line.total}}\n{{/each}}"}
            };
            std::int64_t order = 0;
            for (const auto& seed: seeds) {
                auto existing = db.selectFirst<TicketPlaceholder>(
                    col<&TicketPlaceholder::placeholder> == seed.placeholder);
                if (!existing) {
                    TicketPlaceholder value;
                    value.placeholder = seed.placeholder;
                    value.descriptionKey = seed.descriptionKey;
                    value.templateSource = seed.source;
                    value.sortOrder = order++;
                    db.insert(value);
                    continue;
                }
                existing->descriptionKey = seed.descriptionKey;
                existing->templateSource = seed.source;
                existing->sortOrder = order++;
                existing->active = true;
                db.update(*existing);
            }
        }
    }

    void initializeSchema(Nearon::Database& db, const TextProvider& text) {
        db.transaction([&] {
            db.createTables<Permission, Role, RolePermission, User, Business, Category, Tax, Product, Customer,
                ConsumptionList, ListLine, CashSession, CashMovement, Payment, TicketSeries, Ticket,
                TicketLine, TicketTax, FiscalRecord, FiscalEvent, AuditEvent, TicketPlaceholder>();
            createImmutableTriggers(db);
            seedPermissionsAndRoles(db, text);
            seedCatalogue(db, text);
            seedTicketPlaceholders(db);
            // Legacy installations may contain these two discontinued options.
            // Keep their columns for a safe migration, but normalize all data.
            db.execute("UPDATE categories SET parent_id = NULL WHERE parent_id IS NOT NULL");
            db.execute("UPDATE products SET open_price = 0 WHERE open_price <> 0");
            db.setUserVersion(SchemaVersion);
        });
    }

    std::shared_ptr<Nearon::Database> openHospitalityDatabase(const std::filesystem::path& path, const TextProvider& text) {
        if (path != ":memory:" && path.has_parent_path())
            std::filesystem::create_directories(path.parent_path());
        auto db = std::make_shared<Nearon::Database>(path);
        initializeSchema(*db, text);
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
                              const QuantityMilli quantity, const UserId user) {
        if (quantity <= 0) throw HospitalityError("errors.quantity_positive");
        return db.transaction([&] {
            const auto list = db.get<ConsumptionList>(listId);
            if (list.status != ListStatus::Open) throw HospitalityError("errors.list_not_open");
            const auto product = db.get<Product>(productId);
            if (!product.active) throw HospitalityError("errors.product_inactive");
            const Tax tax = db.get<Tax>(product.taxId);
            if (!tax.active) throw HospitalityError("errors.product_tax_inactive");

            const Cents unitPrice = product.price;
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
                           const Cents amount, const TaxId taxId, const UserId user, const TextProvider& text) {
        if (description.empty()) description = text("lists.manual_amount");
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
