#include "SimulationMenu.h"

#include "database/Database.h"
#include "database/models/Business.h"
#include "database/models/Category.h"
#include "database/models/Product.h"
#include "database/models/Tax.h"
#include "database/models/User.h"
#include "license/License.h"
#include "sales/SaleService.h"
#include "../Management.h"

#include <vector>

void SimulationMenu::init() {
    beginMenu("Simulate a business", "Back", 74);
    setHeaderLines([] {
        return std::vector<HeaderLine>{{"NexoBar Management", ftxui::Color::Cyan},
                                       {"Functional test data", ftxui::Color::BlueLight}};
    });
    addAction("Create or complete the simulated business", [this] { showResult(seed()); });
    setFooterAction("Back", [this] { if (on_back) on_back(); });
}

std::string SimulationMenu::seed() {
    try {
        License licenses;
        auto business = database_.query<nexobar::Business>().first().value_or(nexobar::Business{});
        business.legalName = "NexoBar Demo Ltd.";
        business.tradeName = "The Test Bar";
        business.nif = "B12345678";
        business.address = "1 Main Street";
        business.postalCode = "28001";
        business.city = "Madrid";
        business.province = "Madrid";
        business.country = "ES";
        business.phone = "+34 910 000 000";
        business.email = "demo@nexobar.local";
        business.license = licenses.generate(License::LicenseType::Pro);
        if (business.id.get()) database_.update(business); else business.id = database_.insert(business);

        const auto taxes = database_.query<nexobar::Tax>().where(Nearon::col<&nexobar::Tax::active> == true).all();
        const auto categories = database_.query<nexobar::Category>().where(Nearon::col<&nexobar::Category::active> == true).all();
        const auto users = database_.query<nexobar::User>().where(Nearon::col<&nexobar::User::active> == true).all();
        if (taxes.empty() || categories.empty() || users.empty())
            return "The schema does not contain taxes, categories, or users.";
        struct DemoProduct { std::string name; std::int64_t price; };
        const DemoProduct samples[] = {
            {"Coffee", 150}, {"Soft drink", 250}, {"Beer", 300}, {"Toast", 275}
        };
        std::vector<nexobar::Product> products;
        for (const auto& sample : samples) {
            auto product = database_.selectFirst<nexobar::Product>(Nearon::col<&nexobar::Product::name> == sample.name);
            if (!product) {
                nexobar::Product value;
                value.name = sample.name; value.price = sample.price;
                value.taxId = taxes.front().id; value.categoryId = categories.front().id;
                value.id = database_.insert(value); product = value;
            }
            products.push_back(*product);
        }
        if (database_.count<nexobar::Ticket>() == 0) {
            ManagementImp management;
            nexobar::SaleService sales(database_, [management](const std::string_view key) {
                return management->text(key);
            });
            sales.issue(business, users.front().id, {
                {products[0].id, products[0].name, 2000, products[0].price, taxes.front().rate},
                {products[2].id, products[2].name, 1000, products[2].price, taxes.front().rate}
            });
            sales.issue(business, users.front().id, {
                {products[1].id, products[1].name, 1000, products[1].price, taxes.front().rate},
                {products[3].id, products[3].name, 1000, products[3].price, taxes.front().rate}
            });
        }
        return "Simulation completed successfully. The business, catalogue, users, permissions, and sample sales are ready.";
    } catch (const std::exception& error) {
        ManagementImp management;
        return "The simulation could not be completed: " + management->userError(error);
    }
}
