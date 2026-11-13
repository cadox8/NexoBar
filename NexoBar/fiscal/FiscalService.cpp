#include "FiscalService.h"

#include "security/Sha256.h"
#include "security/XadesSigner.h"
#include "fiscal/SoftwareProducerIdentity.h"
#include "time/AppTime.h"
#include "reporting/CashDay.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>
#include <mutex>

namespace nexobar {
    namespace {
        struct SigningConfiguration {
            std::filesystem::path path;
            std::string password;
            std::filesystem::path schemaDirectory;
        };

        std::mutex signingMutex;
        std::optional<SigningConfiguration> signingConfiguration;
        bool certificateInstalledFlag = false;

        std::optional<SigningConfiguration> signing() {
            std::lock_guard lock(signingMutex);
            return signingConfiguration;
        }

        std::string fiscalDate(const Date& date) {
            std::ostringstream out;
            out << std::setfill('0') << std::setw(2) << static_cast<int>(date.day()) << '-'
                    << std::setw(2) << static_cast<int>(date.month()) << '-' << std::setw(4)
                    << static_cast<int>(date.year());
            return out.str();
        }

        Date invoiceDate(const Ticket& ticket) {
            return AppTime::toLocal(ticket.issuedAt).local_time().date();
        }

        std::string amount(const Cents value) {
            std::ostringstream out;
            const auto absolute = value < 0 ? -value : value;
            if (value < 0) out << '-';
            out << absolute / 100 << '.' << std::setfill('0') << std::setw(2) << absolute % 100;
            return out.str();
        }

        std::string taxRate(const BasisPoints value) {
            std::ostringstream out;
            out << value / 100 << '.' << std::setfill('0') << std::setw(2) << std::abs(value % 100);
            return out.str();
        }

        std::string xmlEscape(std::string_view value) {
            std::string result;
            for (const char c: value) {
                switch (c) {
                    case '&': result += "&amp;";
                        break;
                    case '<': result += "&lt;";
                        break;
                    case '>': result += "&gt;";
                        break;
                    case '\"': result += "&quot;";
                        break;
                    case '\'': result += "&apos;";
                        break;
                    default: result += c;
                }
            }
            return result;
        }

        std::string urlEncode(std::string_view value) {
            std::ostringstream out;
            out << std::uppercase << std::hex;
            for (const unsigned char c: value) {
                if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') out << c;
                else out << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(c);
            }
            return out.str();
        }

        std::string generatedTimestamp() {
            const auto local = AppTime::localNow();
            const auto time = local.local_time();
            const auto offset = local.zone()->base_utc_offset() +
                                (local.is_dst() ? local.zone()->dst_offset() : boost::posix_time::seconds(0));
            const auto seconds = offset.total_seconds();
            const char sign = seconds < 0 ? '-' : '+';
            const auto absolute = std::abs(seconds);
            std::ostringstream out;
            out << boost::gregorian::to_iso_extended_string(time.date()) << 'T'
                    << std::setfill('0') << std::setw(2) << time.time_of_day().hours() << ':'
                    << std::setw(2) << time.time_of_day().minutes() << ':' << std::setw(2)
                    << time.time_of_day().seconds() << sign << std::setw(2) << absolute / 3600
                    << ':' << std::setw(2) << absolute % 3600 / 60;
            return out.str();
        }

        std::string invoiceType(const TicketType type) {
            switch (type) {
                case TicketType::Simplified: return "F2";
                case TicketType::Complete: return "F1";
                case TicketType::RectifyingSimplified: return "R5";
                case TicketType::RectifyingComplete: return "R1";
            }
            return "F2";
        }

        std::string issueFingerprintInputAtDate(const Ticket& ticket, const Date& date,
                                                const std::string_view previousHash,
                                                const std::string_view generatedAt) {
            return "IDEmisorFactura=" + ticket.issuerNif + "&NumSerieFactura=" + ticket.fullNumber +
                   "&FechaExpedicionFactura=" + fiscalDate(date) +
                   "&TipoFactura=" + invoiceType(ticket.type) + "&CuotaTotal=" + amount(ticket.taxAmount) +
                   "&ImporteTotal=" + amount(ticket.total) + "&Huella=" + std::string(previousHash) +
                   "&FechaHoraHusoGenRegistro=" + std::string(generatedAt);
        }

        std::string cancellationFingerprintInputAtDate(const Ticket& ticket, const Date& date,
                                                       const std::string_view previousHash,
                                                       const std::string_view generatedAt) {
            return "IDEmisorFacturaAnulada=" + ticket.issuerNif + "&NumSerieFacturaAnulada=" + ticket.fullNumber +
                   "&FechaExpedicionFacturaAnulada=" + fiscalDate(date) +
                   "&Huella=" + std::string(previousHash) +
                   "&FechaHoraHusoGenRegistro=" + std::string(generatedAt);
        }

        std::string element(const std::string_view name, const std::string_view value) {
            return "<" + std::string(name) + ">" + xmlEscape(value) + "</" + std::string(name) + ">";
        }

        std::string xmlValue(const std::string& xml, const std::string_view name) {
            const std::string opening = "<" + std::string(name) + ">";
            const std::string closing = "</" + std::string(name) + ">";
            const auto begin = xml.find(opening);
            const auto end = begin == std::string::npos ? std::string::npos : xml.find(closing, begin);
            return begin == std::string::npos || end == std::string::npos ? std::string{} : xml.substr(begin + opening.size(), end - begin - opening.size());
        }

        void validateXml(const std::string_view xml, const std::filesystem::path& schema) {
            std::string error;
            if (!XadesSigner::validateSchema(xml, schema, error)) throw HospitalityError(error);
        }
    }

    void FiscalService::configureSigning(std::filesystem::path pkcs12Path, std::string password,
                                         std::filesystem::path schemaDirectory) {
        if (!std::filesystem::is_regular_file(pkcs12Path))
            throw HospitalityError("errors.fiscal_certificate_open");
        if (password.empty()) throw HospitalityError("errors.fiscal_certificate_password_missing");
        if (!std::filesystem::is_regular_file(schemaDirectory / "SuministroInformacion.xsd") ||
            !std::filesystem::is_regular_file(schemaDirectory / "EventosSIF.xsd") ||
            !std::filesystem::is_regular_file(schemaDirectory / "xmldsig-core-schema.xsd"))
            throw HospitalityError("errors.fiscal_schema_missing");
        std::lock_guard lock(signingMutex);
        certificateInstalledFlag = true;
        signingConfiguration = SigningConfiguration{
            std::move(pkcs12Path), std::move(password),
            std::move(schemaDirectory)
        };
    }

    void FiscalService::clearSigning() {
        std::lock_guard lock(signingMutex);
        signingConfiguration.reset();
        certificateInstalledFlag = false;
    }

    bool FiscalService::signingConfigured() {
        return signing().has_value();
    }

    void FiscalService::setCertificateInstalled(const bool installed) {
        std::lock_guard lock(signingMutex);
        certificateInstalledFlag = installed;
    }

    bool FiscalService::certificateInstalled() {
        std::lock_guard lock(signingMutex);
        return certificateInstalledFlag;
    }

    std::string FiscalService::issueFingerprintInput(const Ticket& ticket,
                                                     const std::string_view previousHash,
                                                     const std::string_view generatedAt) {
        return issueFingerprintInputAtDate(ticket, ticket.issuedAt.date(), previousHash,
                                           generatedAt);
    }

    std::string FiscalService::cancellationFingerprintInput(const Ticket& ticket,
                                                            const std::string_view previousHash,
                                                            const std::string_view generatedAt) {
        return cancellationFingerprintInputAtDate(ticket, ticket.issuedAt.date(), previousHash,
                                                  generatedAt);
    }

    std::string FiscalService::qrUrl(const Ticket& ticket) {
        return "https://www2.agenciatributaria.gob.es/wlpl/TIKE-CONT/ValidarQR?nif=" + urlEncode(ticket.issuerNif) + "&numserie=" + urlEncode(ticket.fullNumber) + "&fecha=" + fiscalDate(invoiceDate(ticket)) + "&importe=" + amount(ticket.total);
    }

    FiscalRecord FiscalService::recordIssue(const Business& business, const Ticket& ticket,
                                            const std::vector<TicketTax>& taxes, const UserId user) {
        if (m_database.count<FiscalRecord>(Nearon::col<&FiscalRecord::ticketId> == ticket.id &&
                                           Nearon::col<&FiscalRecord::type> == FiscalRecordType::Issue) != 0)
            throw HospitalityError("errors.fiscal_issue_exists");
        return append(business, ticket, taxes, FiscalRecordType::Issue, user, std::nullopt);
    }

    FiscalRecord FiscalService::cancel(const Business& business, const TicketId ticketId,
                                       const UserId user, std::string reason) {
        if (reason.empty()) throw HospitalityError("errors.cancellation_reason_required");
        return m_database.transaction([&] {
            const Ticket ticket = m_database.get<Ticket>(ticketId);
            if (isTicketCashDayClosed(m_database, ticket))
                throw HospitalityError("errors.ticket_day_closed");
            if (m_database.count<FiscalRecord>(Nearon::col<&FiscalRecord::ticketId> == ticketId &&
                                               Nearon::col<&FiscalRecord::type> == FiscalRecordType::Issue) == 0)
                throw HospitalityError("errors.fiscal_issue_missing");
            if (m_database.count<FiscalRecord>(Nearon::col<&FiscalRecord::ticketId> == ticketId &&
                                               Nearon::col<&FiscalRecord::type> == FiscalRecordType::Cancellation) != 0)
                throw HospitalityError("errors.ticket_already_cancelled");
            const auto taxes = m_database.selectWhere<TicketTax>(Nearon::col<&TicketTax::ticketId> == ticketId);
            auto result = append(business, ticket, taxes, FiscalRecordType::Cancellation, user, std::move(reason));
            audit(m_database, user, "TICKET_CANCELLED", "tickets", ticket.id.get(), ticket.fullNumber);
            return result;
        });
    }

    FiscalRecord FiscalService::append(const Business& business, const Ticket& ticket,
                                       const std::vector<TicketTax>& taxes, const FiscalRecordType type,
                                       const UserId user, std::optional<std::string> reason) {
        const auto verification = verifyChain();
        if (!verification.valid) throw HospitalityError("errors.fiscal_chain_invalid");
        const auto previous = m_database.query<FiscalRecord>()
                .orderBy(Nearon::col<&FiscalRecord::id>.desc()).first();
        const std::string timestamp = generatedTimestamp();
        const std::string previousHash = previous ? previous->hash : std::string{};
        const Date localInvoiceDate = invoiceDate(ticket);
        const std::string canonical = type == FiscalRecordType::Issue
            ? issueFingerprintInputAtDate(ticket, localInvoiceDate, previousHash, timestamp)
            : cancellationFingerprintInputAtDate(ticket, localInvoiceDate, previousHash, timestamp);

        FiscalRecord record;
        record.type = type;
        record.ticketId = ticket.id;
        if (type == FiscalRecordType::Cancellation) record.relatedTicketId = ticket.id;
        record.generatedAt = AppTime::UTCNow();
        record.systemCode = "NEXOBAR";
        record.softwareVersion = "1.0.0";
        record.previousRecordId = previous ? std::optional{previous->id} : std::nullopt;
        record.previousHash = previous ? std::optional{previous->hash} : std::nullopt;
        record.hashAlgorithm = "SHA-256";
        record.hash = sha256(canonical);
        record.signatureFormat = "UNSIGNED_CERTIFICATE_UNAVAILABLE";
        record.invoiceNumber = ticket.fullNumber;
        record.invoiceDate = localInvoiceDate;
        record.issuerNif = ticket.issuerNif;
        record.generatedBy = user;
        record.reason = std::move(reason);
        if (type == FiscalRecordType::Issue) record.qrPayload = qrUrl(ticket);

        std::ostringstream xml;
        xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                << (type == FiscalRecordType::Issue ? "<RegistroAlta" : "<RegistroAnulacion")
                << " xmlns=\"https://www2.agenciatributaria.gob.es/static_files/common/internet/dep/aplicaciones/es/aeat/tike/cont/ws/SuministroInformacion.xsd\">";
        if (type == FiscalRecordType::Issue) {
            xml << element("IDVersion", "1.0") << "<IDFactura>"
                    << element("IDEmisorFactura", ticket.issuerNif)
                    << element("NumSerieFactura", ticket.fullNumber)
                    << element("FechaExpedicionFactura", fiscalDate(invoiceDate(ticket))) << "</IDFactura>"
                    << element("NombreRazonEmisor", ticket.issuerName)
                    << element("TipoFactura", invoiceType(ticket.type));
            if (ticket.operationDate)
                xml << element("FechaOperacion", fiscalDate(*ticket.operationDate));
            xml << element("DescripcionOperacion", "Servicios de hostelería") << "<Desglose>";
            for (const auto& tax: taxes) {
                xml << "<DetalleDesglose>" << element("Impuesto", tax.taxCode) << element("ClaveRegimen", tax.regimeCode);
                if (!tax.operationCode.empty() && tax.operationCode.front() == 'E')
                    xml << element("OperacionExenta", tax.operationCode);
                else
                    xml << element("CalificacionOperacion", tax.operationCode) << element("TipoImpositivo", taxRate(tax.rate));
                xml << element("BaseImponibleOimporteNoSujeto", amount(tax.taxBase));
                if (tax.operationCode.empty() || tax.operationCode.front() != 'E')
                    xml << element("CuotaRepercutida", amount(tax.taxAmount));
                xml << "</DetalleDesglose>";
            }
            xml << "</Desglose>" << element("CuotaTotal", amount(ticket.taxAmount))
                    << element("ImporteTotal", amount(ticket.total));
        } else {
            xml << element("IDVersion", "1.0") << "<IDFactura>"
                    << element("IDEmisorFacturaAnulada", ticket.issuerNif)
                    << element("NumSerieFacturaAnulada", ticket.fullNumber)
                    << element("FechaExpedicionFacturaAnulada", fiscalDate(invoiceDate(ticket)))
                    << "</IDFactura>";
        }
        const std::string producerName(SoftwareProducerIdentity::LegalName);
        const std::string producerNif(SoftwareProducerIdentity::Nif);
        const std::string installation = business.installationNumber.empty() ? "UNCONFIGURED" : business.installationNumber;
        xml << "<Encadenamiento>";
        if (previous) {
            xml << "<RegistroAnterior>" << element("IDEmisorFactura", previous->issuerNif)
                    << element("NumSerieFactura", previous->invoiceNumber)
                    << element("FechaExpedicionFactura", fiscalDate(previous->invoiceDate))
                    << element("Huella", previous->hash) << "</RegistroAnterior>";
        } else xml << element("PrimerRegistro", "S");
        xml << "</Encadenamiento><SistemaInformatico>" << element("NombreRazon", producerName)
                << element("NIF", producerNif) << element("NombreSistemaInformatico", "NexoBar")
                << element("IdSistemaInformatico", "NB") << element("Version", record.softwareVersion)
                << element("NumeroInstalacion", installation) << element("TipoUsoPosibleSoloVerifactu", "N")
                << element("TipoUsoPosibleMultiOT", "N") << element("IndicadorMultiplesOT", "N")
                << "</SistemaInformatico>" << element("FechaHoraHusoGenRegistro", timestamp)
                << element("TipoHuella", "01") << element("Huella", record.hash)
                << (type == FiscalRecordType::Issue ? "</RegistroAlta>" : "</RegistroAnulacion>");
        record.payload = xml.str();
        if (const auto configuration = signing()) {
            validateXml(record.payload, configuration->schemaDirectory / "SuministroInformacion.xsd");
            try {
                const auto signedRecord = XadesSigner::sign(
                    record.payload, configuration->path, configuration->password, timestamp);
                validateXml(signedRecord.xml, configuration->schemaDirectory / "SuministroInformacion.xsd");
                record.payload = signedRecord.xml;
                record.signature.assign(signedRecord.signatureXml.begin(), signedRecord.signatureXml.end());
                record.signatureFormat = "XAdES-EPES Enveloped RSA-SHA256";
            } catch (const std::exception&) {
                // A missing, unreadable, expired, or otherwise unusable optional
                // bar certificate must not prevent ordinary POS operation.
                clearSigning();
            }
        }
        record.id = m_database.insert(record);
        return record;
    }

    std::string FiscalService::fingerprintInput(const FiscalRecord& record) const {
        const auto ticket = m_database.get<Ticket>(record.ticketId);
        const std::string marker = "<FechaHoraHusoGenRegistro>";
        const auto begin = record.payload.find(marker);
        const auto end = begin == std::string::npos ? std::string::npos : record.payload.find("</FechaHoraHusoGenRegistro>", begin);
        if (begin == std::string::npos || end == std::string::npos) return {};
        const auto timestamp = record.payload.substr(begin + marker.size(), end - begin - marker.size());
        return record.type == FiscalRecordType::Issue
                   ? issueFingerprintInputAtDate(ticket, record.invoiceDate,
                                                 record.previousHash.value_or(""), timestamp)
                   : cancellationFingerprintInputAtDate(ticket, record.invoiceDate,
                                                        record.previousHash.value_or(""), timestamp);
    }

    FiscalVerification FiscalService::verifyChain(const bool requireSignatures) const {
        FiscalVerification result;
        const auto records = m_database.query<FiscalRecord>().orderBy(Nearon::col<&FiscalRecord::id>.asc()).all();
        std::optional<FiscalRecord> previous;
        for (const auto& record: records) {
            ++result.checked;
            const bool linkOk = previous
                                    ? record.previousRecordId == previous->id && record.previousHash == previous->hash
                                    : !record.previousRecordId && !record.previousHash;
            std::string signatureError;
            const bool signatureOk = (!requireSignatures && record.signature.empty()) ||
                                     (!record.signature.empty() && XadesSigner::verify(record.payload, signatureError));
            if (!linkOk || fingerprintInput(record).empty() || sha256(fingerprintInput(record)) != record.hash || !signatureOk) {
                result.valid = false;
                result.error = requireSignatures && record.signature.empty()
                                   ? "errors.fiscal_signature_missing"
                                   : "Registro fiscal " + std::to_string(record.id.get()) + " no supera la verificación";
                return result;
            }
            previous = record;
        }
        return result;
    }

    FiscalVerification FiscalService::verifyEventChain(const bool requireSignatures) const {
        FiscalVerification result;
        const auto events = m_database.query<FiscalEvent>().orderBy(Nearon::col<&FiscalEvent::id>.asc()).all();
        std::optional<FiscalEvent> previous;
        for (const auto& event: events) {
            ++result.checked;
            const bool linkOk = previous
                                    ? event.previousEventId == previous->id && event.previousHash == previous->hash
                                    : !event.previousEventId && !event.previousHash;
            const auto marker = event.payload.find("<!--canonical:");
            const auto end = marker == std::string::npos ? std::string::npos : event.payload.find("-->", marker);
            std::string signatureError;
            const bool signatureOk = (!requireSignatures && event.signature.empty()) ||
                                     (!event.signature.empty() && XadesSigner::verify(event.payload, signatureError));
            if (!linkOk || end == std::string::npos ||
                sha256(event.payload.substr(marker + 14, end - marker - 14)) != event.hash || !signatureOk) {
                result.valid = false;
                result.error = requireSignatures && event.signature.empty()
                                   ? "errors.fiscal_signature_missing"
                                   : "Registro de evento " + std::to_string(event.id.get()) + " no supera la verificación";
                return result;
            }
            previous = event;
        }
        return result;
    }

    FiscalEvent FiscalService::recordEvent(const Business& business, std::string eventCode,
                                           const std::optional<UserId> user, std::string details) {
        const auto verification = verifyEventChain();
        if (!verification.valid) throw HospitalityError("errors.fiscal_event_chain_invalid");
        const auto previous = m_database.query<FiscalEvent>().orderBy(Nearon::col<&FiscalEvent::id>.desc()).first();
        const auto timestamp = generatedTimestamp();
        const auto previousHash = previous ? previous->hash : std::string{};
        const std::string producerName(SoftwareProducerIdentity::LegalName);
        const std::string producerNif(SoftwareProducerIdentity::Nif);
        const std::string installation = business.installationNumber.empty() ? "UNCONFIGURED" : business.installationNumber;
        const std::string canonical = "NIF=" + producerNif + "&ID=&IdSistemaInformatico=NB&Version=1.0.0"
                                      "&NumeroInstalacion=" + installation + "&NIF=" + business.nif + "&TipoEvento=" + eventCode +
                                      "&HuellaEvento=" + previousHash + "&FechaHoraHusoGenEvento=" + timestamp;
        FiscalEvent event;
        event.eventCode = std::move(eventCode);
        event.generatedAt = AppTime::UTCNow();
        event.userId = user;
        event.previousEventId = previous ? std::optional{previous->id} : std::nullopt;
        event.previousHash = previous ? std::optional{previous->hash} : std::nullopt;
        event.hashAlgorithm = "SHA-256";
        event.hash = sha256(canonical);
        event.signatureFormat = "UNSIGNED_CERTIFICATE_UNAVAILABLE";
        std::ostringstream xml;
        xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                << "<RegistroEvento xmlns=\"https://www2.agenciatributaria.gob.es/static_files/common/internet/dep/aplicaciones/es/aeat/tike/cont/ws/EventosSIF.xsd\"><IDVersion>1.0</IDVersion><Evento>"
                << "<SistemaInformatico>" << element("NombreRazon", producerName) << element("NIF", producerNif)
                << element("NombreSistemaInformatico", "NexoBar") << element("IdSistemaInformatico", "NB")
                << element("Version", "1.0.0") << element("NumeroInstalacion", installation)
                << element("TipoUsoPosibleSoloVerifactu", "N") << element("TipoUsoPosibleMultiOT", "N")
                << element("IndicadorMultiplesOT", "N") << "</SistemaInformatico><ObligadoEmision>"
                << element("NombreRazon", business.legalName) << element("NIF", business.nif) << "</ObligadoEmision>"
                << element("FechaHoraHusoGenEvento", timestamp) << element("TipoEvento", event.eventCode);
        if (event.eventCode == "03") {
            const auto checked = m_database.count<FiscalRecord>();
            xml << "<DatosPropiosEvento><LanzamientoProcesoDeteccionAnomaliasRegFacturacion>"
                    << element("RealizadoProcesoSobreIntegridadHuellasRegFacturacion", "S")
                    << element("NumeroDeRegistrosFacturacionProcesadosSobreIntegridadHuellas", std::to_string(checked))
                    << element("RealizadoProcesoSobreIntegridadFirmasRegFacturacion", "S")
                    << element("NumeroDeRegistrosFacturacionProcesadosSobreIntegridadFirmas", std::to_string(checked))
                    << element("RealizadoProcesoSobreTrazabilidadCadenaRegFacturacion", "S")
                    << element("NumeroDeRegistrosFacturacionProcesadosSobreTrazabilidadCadena", std::to_string(checked))
                    << element("RealizadoProcesoSobreTrazabilidadFechasRegFacturacion", "S")
                    << element("NumeroDeRegistrosFacturacionProcesadosSobreTrazabilidadFechas", std::to_string(checked))
                    << "</LanzamientoProcesoDeteccionAnomaliasRegFacturacion></DatosPropiosEvento>";
        } else if (event.eventCode == "05") {
            const auto checked = m_database.count<FiscalEvent>();
            xml << "<DatosPropiosEvento><LanzamientoProcesoDeteccionAnomaliasRegEvento>"
                    << element("RealizadoProcesoSobreIntegridadHuellasRegEvento", "S")
                    << element("NumeroDeRegistrosEventoProcesadosSobreIntegridadHuellas", std::to_string(checked))
                    << element("RealizadoProcesoSobreIntegridadFirmasRegEvento", "S")
                    << element("NumeroDeRegistrosEventoProcesadosSobreIntegridadFirmas", std::to_string(checked))
                    << element("RealizadoProcesoSobreTrazabilidadCadenaRegEvento", "S")
                    << element("NumeroDeRegistrosEventoProcesadosSobreTrazabilidadCadena", std::to_string(checked))
                    << element("RealizadoProcesoSobreTrazabilidadFechasRegEvento", "S")
                    << element("NumeroDeRegistrosEventoProcesadosSobreTrazabilidadFechas", std::to_string(checked))
                    << "</LanzamientoProcesoDeteccionAnomaliasRegEvento></DatosPropiosEvento>";
        } else if (event.eventCode == "04") {
            xml << "<DatosPropiosEvento><DeteccionAnomaliasRegFacturacion>"
                    << element("TipoAnomalia", "01")
                    << element("OtrosDatosAnomalia", details.substr(0, 100))
                    << "</DeteccionAnomaliasRegFacturacion></DatosPropiosEvento>";
        } else if (event.eventCode == "06") {
            xml << "<DatosPropiosEvento><DeteccionAnomaliasRegEvento>"
                    << element("TipoAnomalia", "01")
                    << element("OtrosDatosAnomalia", details.substr(0, 100))
                    << "</DeteccionAnomaliasRegEvento></DatosPropiosEvento>";
        } else if (event.eventCode == "08" && details.find('/') != std::string::npos) {
            const auto separator = details.find('/');
            const auto startDate = boost::gregorian::from_simple_string(details.substr(0, separator));
            const auto endDate = boost::gregorian::from_simple_string(details.substr(separator + 1));
            std::vector<FiscalRecord> exported;
            std::size_t issueCount = 0;
            std::size_t cancellationCount = 0;
            Cents taxTotal = 0;
            Cents grandTotal = 0;
            for (const auto& record: m_database.query<FiscalRecord>().orderBy(Nearon::col<&FiscalRecord::id>.asc()).all()) {
                if (record.invoiceDate < startDate || record.invoiceDate > endDate) continue;
                exported.push_back(record);
                if (record.type == FiscalRecordType::Issue) {
                    ++issueCount;
                    const auto ticket = m_database.get<Ticket>(record.ticketId);
                    taxTotal += ticket.taxAmount;
                    grandTotal += ticket.total;
                } else ++cancellationCount;
            }
            if (exported.empty()) throw HospitalityError("errors.fiscal_export_empty");
            const auto recordIdentity = [](const FiscalRecord& record) {
                return element("IDEmisorFactura", record.issuerNif) +
                       element("NumSerieFactura", record.invoiceNumber) +
                       element("FechaExpedicionFactura", fiscalDate(record.invoiceDate)) + element("Huella", record.hash);
            };
            const std::string offset = timestamp.substr(timestamp.size() - 6);
            xml << "<DatosPropiosEvento><ExportacionRegFacturacionPeriodo>"
                    << element("FechaHoraHusoInicioPeriodoExport", boost::gregorian::to_iso_extended_string(startDate) + "T00:00:00" + offset)
                    << element("FechaHoraHusoFinPeriodoExport", boost::gregorian::to_iso_extended_string(endDate) + "T23:59:59" + offset)
                    << "<RegistroFacturacionInicialPeriodo>" << recordIdentity(exported.front()) << "</RegistroFacturacionInicialPeriodo>"
                    << "<RegistroFacturacionFinalPeriodo>" << recordIdentity(exported.back()) << "</RegistroFacturacionFinalPeriodo>"
                    << element("NumeroDeRegistrosFacturacionAltaExportados", std::to_string(issueCount))
                    << element("SumaCuotaTotalAlta", amount(taxTotal))
                    << element("SumaImporteTotalAlta", amount(grandTotal))
                    << element("NumeroDeRegistrosFacturacionAnulacionExportados", std::to_string(cancellationCount))
                    << element("RegistrosFacturacionExportadosDejanDeConservarse", "N")
                    << "</ExportacionRegFacturacionPeriodo></DatosPropiosEvento>";
        } else if (event.eventCode == "10") {
            std::map<std::string, std::size_t> eventCounts;
            std::optional<Timestamp> periodStart;
            const auto existingEvents = m_database.query<FiscalEvent>().orderBy(
                Nearon::col<&FiscalEvent::id>.asc()).all();
            for (auto iterator = existingEvents.rbegin(); iterator != existingEvents.rend(); ++iterator) {
                if (iterator->eventCode == "10") {
                    periodStart = iterator->generatedAt;
                    break;
                }
            }
            for (const auto& existing: existingEvents)
                if (!periodStart || existing.generatedAt > *periodStart) ++eventCounts[existing.eventCode];
            if (eventCounts.empty()) eventCounts["10"] = 1;
            std::size_t issueCount = 0;
            std::size_t cancellationCount = 0;
            Cents taxTotal = 0;
            Cents grandTotal = 0;
            std::vector<FiscalRecord> periodRecords;
            for (const auto& record: m_database.query<FiscalRecord>().orderBy(
                     Nearon::col<&FiscalRecord::id>.asc()).all()) {
                if (periodStart && record.generatedAt <= *periodStart) continue;
                periodRecords.push_back(record);
                if (record.type == FiscalRecordType::Issue) {
                    ++issueCount;
                    const auto ticket = m_database.get<Ticket>(record.ticketId);
                    taxTotal += ticket.taxAmount;
                    grandTotal += ticket.total;
                } else ++cancellationCount;
            }
            xml << "<DatosPropiosEvento><ResumenEventos>";
            for (const auto& [code, count]: eventCounts)
                xml << "<TipoEvento>" << element("TipoEvento", code)
                        << element("NumeroDeEventos", std::to_string(count)) << "</TipoEvento>";
            if (!periodRecords.empty()) {
                const auto recordIdentity = [](const FiscalRecord& record) {
                    return element("IDEmisorFactura", record.issuerNif) +
                           element("NumSerieFactura", record.invoiceNumber) +
                           element("FechaExpedicionFactura", fiscalDate(record.invoiceDate)) + element("Huella", record.hash);
                };
                xml << "<RegistroFacturacionInicialPeriodo>" << recordIdentity(periodRecords.front())
                        << "</RegistroFacturacionInicialPeriodo><RegistroFacturacionFinalPeriodo>"
                        << recordIdentity(periodRecords.back()) << "</RegistroFacturacionFinalPeriodo>";
            }
            xml << element("NumeroDeRegistrosFacturacionAltaGenerados", std::to_string(issueCount))
                    << element("SumaCuotaTotalAlta", amount(taxTotal))
                    << element("SumaImporteTotalAlta", amount(grandTotal))
                    << element("NumeroDeRegistrosFacturacionAnulacionGenerados", std::to_string(cancellationCount))
                    << "</ResumenEventos></DatosPropiosEvento>";
        } else if (!details.empty()) xml << element("OtrosDatosEvento", details.substr(0, 100));
        xml << "<Encadenamiento>";
        if (previous)
            xml << "<EventoAnterior>" << element("TipoEvento", previous->eventCode)
                    << element("FechaHoraHusoGenEvento", xmlValue(previous->payload, "FechaHoraHusoGenEvento"))
                    << element("HuellaEvento", previous->hash) << "</EventoAnterior>";
        else xml << element("PrimerEvento", "S");
        xml << "</Encadenamiento>" << element("TipoHuella", "01") << element("HuellaEvento", event.hash)
                << "</Evento></RegistroEvento><!--canonical:" << canonical << "-->";
        event.payload = xml.str();
        if (const auto configuration = signing()) {
            try {
                const auto signedEvent = XadesSigner::sign(
                    event.payload, configuration->path, configuration->password, timestamp);
                validateXml(signedEvent.xml, configuration->schemaDirectory / "EventosSIF.xsd");
                event.payload = signedEvent.xml;
                event.signature.assign(signedEvent.signatureXml.begin(), signedEvent.signatureXml.end());
                event.signatureFormat = "XAdES-EPES Enveloped RSA-SHA256";
            } catch (const std::exception&) {
                clearSigning();
            }
        }
        event.id = m_database.insert(event);
        return event;
    }

    std::filesystem::path FiscalService::exportPeriod(const Date& from, const Date& through,
                                                      const FiscalExportFormat format,
                                                      const std::filesystem::path& root) {
        if (through < from) throw HospitalityError("errors.fiscal_period_invalid");
        const bool xmlExport = format == FiscalExportFormat::Xml;
        if (xmlExport && !signingConfigured())
            throw HospitalityError("errors.fiscal_certificate_missing");
        const auto verification = verifyChain(xmlExport);
        if (!verification.valid && verification.error == "errors.fiscal_signature_missing")
            throw HospitalityError("errors.fiscal_signature_missing");
        if (!verification.valid) throw HospitalityError("errors.fiscal_chain_invalid");
        const auto records = m_database.query<FiscalRecord>().orderBy(Nearon::col<&FiscalRecord::id>.asc()).all();
        if (xmlExport && std::ranges::any_of(records, [&](const FiscalRecord& record) {
            return record.invoiceDate >= from && record.invoiceDate <= through && record.signature.empty();
        }))
            throw HospitalityError("errors.fiscal_signature_missing");
        const auto directory = root / (boost::gregorian::to_iso_string(from) + "_" + boost::gregorian::to_iso_string(through));
        std::filesystem::create_directories(directory);
        nlohmann::json json = nlohmann::json::array();
        std::size_t count = 0;
        for (const auto& record: records) {
            if (record.invoiceDate < from || record.invoiceDate > through) continue;
            ++count;
            if (format == FiscalExportFormat::Xml) {
                const auto path = directory / (std::to_string(record.id.get()) + "_" +
                                               (record.type == FiscalRecordType::Issue ? "alta.xml" : "anulacion.xml"));
                std::ofstream output(path, std::ios::binary | std::ios::trunc);
                output.exceptions(std::ios::failbit | std::ios::badbit);
                output << record.payload;
            } else {
                json.push_back({
                    {"id", record.id.get()}, {"tipo", record.type == FiscalRecordType::Issue ? "ALTA" : "ANULACION"},
                    {"factura", record.invoiceNumber}, {"fecha_factura", boost::gregorian::to_iso_extended_string(record.invoiceDate)},
                    {"nif_emisor", record.issuerNif}, {"huella_anterior", record.previousHash.value_or("")},
                    {"huella", record.hash}, {"algoritmo", record.hashAlgorithm}, {"xml", record.payload}
                });
            }
        }
        if (format == FiscalExportFormat::Json) {
            const auto path = directory / "registros_fiscales.json";
            std::ofstream output(path, std::ios::binary | std::ios::trunc);
            output.exceptions(std::ios::failbit | std::ios::badbit);
            output << std::setw(2) << json;
        }
        if (count == 0) throw HospitalityError("errors.fiscal_export_empty");
        if (const auto business = m_database.query<Business>().first())
            recordEvent(*business, "08", std::nullopt,
                        boost::gregorian::to_iso_extended_string(from) + "/" + boost::gregorian::to_iso_extended_string(through));
        return directory;
    }

    std::filesystem::path FiscalService::exportResponsibleDeclaration(const Business& business,
                                                                      const std::filesystem::path& root) {
        if (!SoftwareProducerIdentity::declarationDataComplete() || business.installationNumber.empty())
            throw HospitalityError("errors.fiscal_producer_data_missing");
        std::filesystem::create_directories(root);
        const auto path = root / "declaracion_responsable_NexoBar_1.0.0.txt";
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output.exceptions(std::ios::failbit | std::ios::badbit);
        output << "DECLARACIÓN RESPONSABLE DEL SISTEMA INFORMÁTICO DE FACTURACIÓN\n\n"
                << "1.a) Nombre del sistema informático: NexoBar\n"
                << "1.b) Código identificador: NB\n"
                << "1.c) Identificador completo de versión: 1.0.0\n"
                << "1.d) Componentes y funcionalidades:\n"
                "Software TPV de hostelería instalable en un equipo Windows o Linux, con base de datos "
                "SQLite local y soporte de impresora térmica. Gestiona catálogo, ventas, cobros, facturas "
                "simplificadas y completas, cierres, anulaciones mediante registros de anulación, QR "
                "tributario, registros fiscales y de evento encadenados, firma XAdES, verificación y "
                "exportación XML/JSON. La expedición y el alta fiscal se consolidan en una transacción.\n"
                << "Características de esta instalación: número " << business.installationNumber
                << "; instalación local para un único obligado tributario.\n"
                << "1.e) Uso exclusivo VERI*FACTU: N - No. Modalidad NO VERI*FACTU.\n"
                << "1.f) Uso por varios obligados tributarios: N - No.\n"
                << "1.g) Firma: XAdES Enveloped Signature, nivel EPES, RSA-SHA256, política AGE, "
                "con certificado electrónico cualificado del almacén del sistema operativo.\n"
                << "1.h) Razón social o nombre del productor: " << SoftwareProducerIdentity::LegalName << "\n"
                << "1.i) NIF español del productor: " << SoftwareProducerIdentity::Nif << "\n"
                << "1.j) Dirección postal completa del productor: " << SoftwareProducerIdentity::PostalAddress << ".\n"
                << "1.k) La persona o entidad productora hace constar que NexoBar 1.0.0 cumple lo "
                "dispuesto en el artículo 29.2.j) de la Ley 58/2003, de 17 de diciembre, General "
                "Tributaria, en el Reglamento aprobado por el Real Decreto 1007/2023, de 5 de "
                "diciembre, en la Orden HAC/1177/2024, de 17 de octubre, y en las especificaciones "
                "que las completan publicadas en la sede electrónica de la AEAT.\n"
                << "1.l) Fecha: "
                << boost::gregorian::to_iso_extended_string(boost::gregorian::day_clock::local_day())
                << ". Lugar: " << SoftwareProducerIdentity::City << " (" << SoftwareProducerIdentity::Province
                << "), " << SoftwareProducerIdentity::Country << ".\n\n"
                << "ANEXO\n2.a) Teléfono del productor: " << SoftwareProducerIdentity::Phone
                << ". Correo electrónico del productor: " << SoftwareProducerIdentity::Email << ".\n"
                << "2.c) Implementación: SHA-256 y encadenamiento conforme a AEAT; XAdES-EPES; XSD "
                "oficiales locales; registros append-only; eventos y resúmenes periódicos; conservación "
                "en SQLite; comprobación accesible de huellas, cadena y firmas; exportación por periodo.\n\n"
                << "Firma del productor: ______________________________\n";
        return path;
    }
}
