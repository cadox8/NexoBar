#pragma once

#include "../hospitality/Types.h"
#include "../Orm.h"
#include "User.h"

#include <optional>
#include <string>

namespace nexobar {
    /// Append-only event log required for a non-verifiable SIF mode.
    class FiscalEvent {
    public:
        FiscalEventId id{};
        std::string eventCode;
        Timestamp generatedAt;
        std::optional<UserId> userId;
        std::string payload;
        std::optional<FiscalEventId> previousEventId;
        std::optional<std::string> previousHash;
        std::string hashAlgorithm;
        std::string hash;
        std::string signatureFormat;
        Nearon::Blob signature;
    };

    using FiscalEventTable = Nearon::Table<"fiscal_events", FiscalEvent,
        Nearon::Column<"id", &FiscalEvent::id, Nearon::PrimaryKey, Nearon::AutoIncrement>,
        Nearon::Column<"event_code", &FiscalEvent::eventCode>,
        Nearon::Column<"generated_at", &FiscalEvent::generatedAt>,
        Nearon::Column<"user_id", &FiscalEvent::userId, Nearon::ForeignKey<&User::id>>,
        Nearon::Column<"payload", &FiscalEvent::payload>,
        Nearon::Column<"previous_event_id", &FiscalEvent::previousEventId, Nearon::ForeignKey<&FiscalEvent::id>>,
        Nearon::Column<"previous_hash", &FiscalEvent::previousHash>,
        Nearon::Column<"hash_algorithm", &FiscalEvent::hashAlgorithm>,
        Nearon::Column<"hash", &FiscalEvent::hash>,
        Nearon::Column<"signature_format", &FiscalEvent::signatureFormat>,
        Nearon::Column<"signature", &FiscalEvent::signature>,
        Nearon::Index<"idx_fiscal_events_generated_at", &FiscalEvent::generatedAt>>;
    NEARON_TABLE(FiscalEventTable);
}
