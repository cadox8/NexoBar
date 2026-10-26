#pragma once

#include "../Database.h"
#include "../Models.h"
#include "../TicketPrinter.h"
#include <optional>
#include <string>
#include <vector>

namespace tpv {

class TPV {
public:
    explicit TPV(const std::string& databasePath);
    Database& database() noexcept { return db_; }

    Id createArticle(const std::string& name, Cents priceCents);
    void updateArticle(Id id, const std::string& name, Cents priceCents);
    void deactivateArticle(Id id);
    std::vector<Article> listArticles(bool activeOnly = true) const;

    Id createSale();
    void addArticle(Id saleId, Id articleId, std::int64_t quantityMilli = 1000);
    void addCustomItem(Id saleId, const std::string& description, Cents unitPriceCents,
                       std::int64_t quantityMilli = 1000);
    Sale getSale(Id saleId) const;
    PaymentResult pay(Id saleId, PaymentMethod method, Cents tenderedCents);
    PaymentResult quickCharge(Cents amountCents, PaymentMethod method, Cents tenderedCents,
                              const std::string& description = "Cobro manual");

    Id openCashRegister(Cents openingCents);
    void cashIn(Cents amountCents, const std::string& reason);
    void cashOut(Cents amountCents, const std::string& reason);
    CashSummary cashSummary() const;
    void closeCashRegister(Cents countedCents);

    TicketPrinter& printer() noexcept { return printer_; }
    bool printTicket(Id saleId, const std::string& businessName, std::string& error,
                     const std::string& footer = "Gracias por su visita");

private:
    Database db_;
    TicketPrinter printer_;
    Id activeCashSession() const;
    void recalculateSale(Id saleId);
    void requireOpenSale(Id saleId) const;
};

} // namespace tpv
