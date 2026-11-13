#pragma once

#include "database/models/Business.h"
#include "database/hospitality/HospitalityDatabase.h"
#include "database/models/Ticket.h"
#include "database/models/TicketLine.h"

#include <optional>
#include <string>
#include <utility>
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
        std::string taxCode{"01"};
        std::string regimeCode{"01"};
        std::string operationCode{"S1"};
    };

    struct IssuedSale {
        Ticket ticket;
        std::vector<TicketLine> lines;
    };

    // Issues an immutable simplified ticket and all its lines/tax summaries
    // atomically.
    class SaleService {
    public:
        SaleService(Nearon::Database& database, TextProvider text)
            : m_database(database), m_text(std::move(text)) {}

        IssuedSale issue(const Business& business, UserId user,
                         const std::vector<SaleItem>& items,
                         std::optional<ConsumptionListId> list = std::nullopt);

    private:
        Nearon::Database& m_database;
        TextProvider m_text;
    };
} // namespace nexobar
