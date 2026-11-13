#pragma once

#include "database/hospitality/HospitalityDatabase.h"
#include "database/models/Business.h"
#include "database/models/FiscalRecord.h"
#include "database/models/Ticket.h"
#include "database/models/TicketTax.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace nexobar {
    enum class FiscalExportFormat { Xml, Json };

    struct FiscalVerification {
        bool valid = true;
        std::size_t checked = 0;
        std::string error;
    };

    class FiscalService {
    public:
        FiscalService(Nearon::Database& database, TextProvider text)
            : m_database(database), m_text(std::move(text)) {}

        static void configureSigning(std::filesystem::path pkcs12Path, std::string password,
                                     std::filesystem::path schemaDirectory);
        static void clearSigning();
        [[nodiscard]] static bool signingConfigured();
        static void setCertificateInstalled(bool installed);
        [[nodiscard]] static bool certificateInstalled();

        FiscalRecord recordIssue(const Business& business, const Ticket& ticket,
                                 const std::vector<TicketTax>& taxes, UserId user);
        FiscalRecord cancel(const Business& business, TicketId ticketId, UserId user,
                            std::string reason);
        [[nodiscard]] FiscalVerification verifyChain(bool requireSignatures = false) const;
        [[nodiscard]] FiscalVerification verifyEventChain(bool requireSignatures = false) const;
        FiscalEvent recordEvent(const Business& business, std::string eventCode,
                                std::optional<UserId> user = std::nullopt,
                                std::string details = {});
        std::filesystem::path exportPeriod(const boost::gregorian::date& from,
                                           const boost::gregorian::date& through,
                                           FiscalExportFormat format,
                                           const std::filesystem::path& root = "exports/fiscal");
        static std::filesystem::path exportResponsibleDeclaration(
            const Business& business,
            const std::filesystem::path& root = "exports/fiscal");

        static std::string issueFingerprintInput(const Ticket& ticket,
                                                 std::string_view previousHash,
                                                 std::string_view generatedAt);
        static std::string cancellationFingerprintInput(const Ticket& ticket,
                                                        std::string_view previousHash,
                                                        std::string_view generatedAt);
        static std::string qrUrl(const Ticket& ticket);

    private:
        FiscalRecord append(const Business& business, const Ticket& ticket,
                            const std::vector<TicketTax>& taxes, FiscalRecordType type,
                            UserId user, std::optional<std::string> reason);
        [[nodiscard]] std::string fingerprintInput(const FiscalRecord& record) const;

        Nearon::Database& m_database;
        TextProvider m_text;
    };
}
