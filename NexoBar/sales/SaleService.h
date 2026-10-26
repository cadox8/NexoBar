#pragma once

#include "database/models/Business.h"
#include "database/models/Ticket.h"
#include "database/models/TicketLine.h"

#include <optional>
#include <string>
#include <vector>

namespace Nearon {
    class Database;
}

namespace nexobar {
    struct SaleItem {
        std::optional<ProductId> productId;
        std::string description;
        QuantityMilli quantity{1000};
        Cents unitPrice{};
        BasisPoints taxRate{};
    };

    struct IssuedSale {
        Ticket ticket;
        std::vector<TicketLine> lines;
    };

    // Issues an immutable simplified ticket and all its lines/tax summaries
    // atomically.
    class SaleService {
    public:
        explicit SaleService(Nearon::Database& database) : m_database(database) {}

        IssuedSale issue(const Business& business, UserId user,
                         const std::vector<SaleItem>& items);

    private:
        Nearon::Database& m_database;
    };
} // namespace nexobar
