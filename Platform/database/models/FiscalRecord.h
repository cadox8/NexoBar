#pragma once

#include "../hospitality/Types.h"
#include "../Orm.h"
#include "Ticket.h"

#include <optional>
#include <string>

namespace nexobar {
    /// Append-only NO VERI*FACTU issuance/cancellation record and chain data.
    /// This is a persistence shape; official serialization, hashing and XAdES signing are separate requirements.
    class FiscalRecord {
    public:
        FiscalRecordId id{};
        FiscalRecordType type{FiscalRecordType::Issue};
        TicketId ticketId{};
        std::optional<TicketId> relatedTicketId;
        Timestamp generatedAt;
        std::string systemCode;
        std::string softwareVersion;
        std::string payload;
        std::optional<FiscalRecordId> previousRecordId;
        std::optional<std::string> previousHash;
        std::string hashAlgorithm;
        std::string hash;
        std::string signatureFormat;
        Nearon::Blob signature;
        std::optional<std::string> qrPayload;
    };

    using FiscalRecordTable = Nearon::Table<"fiscal_records", FiscalRecord,
        Nearon::Column<"id", &FiscalRecord::id, Nearon::PrimaryKey, Nearon::AutoIncrement>,
        Nearon::Column<"type", &FiscalRecord::type>,
        Nearon::Column<"ticket_id", &FiscalRecord::ticketId, Nearon::ForeignKey<&Ticket::id>>,
        Nearon::Column<"related_ticket_id", &FiscalRecord::relatedTicketId, Nearon::ForeignKey<&Ticket::id>>,
        Nearon::Column<"generated_at", &FiscalRecord::generatedAt>,
        Nearon::Column<"system_code", &FiscalRecord::systemCode>,
        Nearon::Column<"software_version", &FiscalRecord::softwareVersion>,
        Nearon::Column<"payload", &FiscalRecord::payload>,
        Nearon::Column<"previous_record_id", &FiscalRecord::previousRecordId, Nearon::ForeignKey<&FiscalRecord::id>>,
        Nearon::Column<"previous_hash", &FiscalRecord::previousHash>,
        Nearon::Column<"hash_algorithm", &FiscalRecord::hashAlgorithm>,
        Nearon::Column<"hash", &FiscalRecord::hash>,
        Nearon::Column<"signature_format", &FiscalRecord::signatureFormat>,
        Nearon::Column<"signature", &FiscalRecord::signature>,
        Nearon::Column<"qr_payload", &FiscalRecord::qrPayload>,
        Nearon::UniqueKey<&FiscalRecord::ticketId, &FiscalRecord::type>,
        Nearon::Index<"idx_fiscal_records_generated_at", &FiscalRecord::generatedAt>>;
    NEARON_TABLE(FiscalRecordTable);
}
