#include "TicketTemplate.h"
#include "printer/TicketPrinter.h"
#include "printer/EscPosText.h"
#include "database/hospitality/Money.h"
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#include "NexoBar.h"
#include "log/Log.h"
#include "database/models/FiscalRecord.h"
#include "database/models/TicketTax.h"
#include "fiscal/SoftwareProducerIdentity.h"
#include "fiscal/FiscalService.h"
#include "time/AppTime.h"

namespace {
    using namespace nexobar;

    void replaceAll(std::string& text, const std::string& token, const std::string& value);

    std::string finishJoinedRows(std::string text, const ReceiptLayout* layout) {
        constexpr char joinMarker = '\x1f';
        std::size_t marker = 0;
        while ((marker = text.find(joinMarker, marker)) != std::string::npos) {
            const bool centered = marker + 1 < text.size() && text[marker + 1] == 'C';
            const std::size_t lineStartPosition = text.rfind('\n', marker);
            const std::size_t lineStart = lineStartPosition == std::string::npos ? 0 : lineStartPosition + 1;
            text.erase(marker, std::min<std::size_t>(2, text.size() - marker));
            while (marker > lineStart && (text[marker - 1] == ' ' || text[marker - 1] == '\t')) {
                text.erase(--marker, 1);
            }
            if (!centered || !layout) continue;
            const std::size_t lineEnd = text.find('\n', marker);
            const std::size_t end = lineEnd == std::string::npos ? text.size() : lineEnd;
            std::string line = text.substr(lineStart, end - lineStart);
            const auto first = line.find_first_not_of(" \t\r");
            const auto last = line.find_last_not_of(" \t\r");
            line = first == std::string::npos ? std::string{} : line.substr(first, last - first + 1);
            const auto centeredLine = layout->center(line);
            text.replace(lineStart, end - lineStart, centeredLine);
            marker = lineStart + centeredLine.size();
        }
        return text;
    }

    std::string escPosQr(const std::string& value) {
        if (value.empty()) return {};
        std::string bytes;
        bytes.append("\x1b\x61\x01", 3);
        bytes.append("\x1d\x28\x6b\x04\x00\x31\x41\x32\x00", 9);
        bytes.append("\x1d\x28\x6b\x03\x00\x31\x43\x05", 8);
        bytes.append("\x1d\x28\x6b\x03\x00\x31\x45\x31", 8);
        const auto length = static_cast<unsigned>(value.size() + 3);
        bytes.append("\x1d\x28\x6b", 3);
        bytes.push_back(static_cast<char>(length & 0xff));
        bytes.push_back(static_cast<char>(length >> 8 & 0xff));
        bytes.append("\x31\x50\x30", 3);
        bytes += value;
        bytes.append("\x1d\x28\x6b\x03\x00\x31\x51\x30", 8);
        bytes.push_back('\n');
        bytes.append("\x1b\x61\x00", 3);
        return bytes;
    }

    std::string renderContext(std::string text, const TicketTemplate::TemplateData& data) {
        // Editor metadata is stored as Handlebars-style comments. It preserves
        // block order across restarts but must never reach the physical ticket.
        std::size_t comment = 0;
        while ((comment = text.find("{{!", comment)) != std::string::npos) {
            const std::size_t end = text.find("}}", comment + 3);
            if (end == std::string::npos)
                break;
            const std::string metadata = text.substr(comment, end + 2 - comment);
            const bool sameRow = metadata.starts_with("{{!block:") &&
                                 metadata.ends_with("|same-row}}");
            std::size_t length = end + 2 - comment;
            if (comment + length < text.size() && text[comment + length] == '\n')
                ++length;
            if (sameRow) {
                const std::size_t lineStartPosition = text.rfind('\n', comment);
                const std::size_t lineStart = lineStartPosition == std::string::npos ? 0 : lineStartPosition + 1;
                const std::string_view prefix(text.data() + lineStart, comment - lineStart);
                const bool centered = prefix.find("{{BUSINESS_NAME}}") != std::string_view::npos ||
                                      prefix.find("{{business.name}}") != std::string_view::npos ||
                                      prefix.find("{{ticket.footer}}") != std::string_view::npos ||
                                      prefix.find("{{close.title}}") != std::string_view::npos ||
                                      prefix.find("{{close.date}}") != std::string_view::npos;
                const std::string separator = std::string(1, '\x1f') + (centered ? "C - " : "L - ");
                text.replace(comment, length, separator);
                comment += separator.size();
            } else {
                text.erase(comment, length);
            }
        }
        for (const auto& [name,rows]: data.sections) {
            const std::string open = "{{#each " + name + "}}";
            const std::string close = "{{/each}}";
            std::size_t start = 0;
            while ((start = text.find(open, start)) != std::string::npos) {
                const std::size_t end = text.find(close, start + open.size());
                if (end == std::string::npos)break;
                const std::string body = text.substr(start + open.size(), end - start - open.size());
                std::string rendered;
                for (const auto& row: rows)rendered += renderContext(body, row);
                text.replace(start, end + close.size() - start, rendered);
                start += rendered.size();
            }
        }
        for (const auto& [name,rows]: data.sections) {
            const std::string open = "{{#" + name + "}}", close = "{{/" + name + "}}";
            std::size_t start = 0;
            while ((start = text.find(open, start)) != std::string::npos) {
                const std::size_t end = text.find(close, start + open.size());
                if (end == std::string::npos)break;
                const std::string body = text.substr(start + open.size(), end - start - open.size());
                std::string rendered;
                for (const auto& row: rows)rendered += renderContext(body, row);
                text.replace(start, end + close.size() - start, rendered);
                start += rendered.size();
            }
        }
        for (const auto& [key,value]: data.values)replaceAll(text, "{{" + key + "}}", value);
        return text;
    }

    void replaceAll(std::string& text, const std::string& token, const std::string& value) {
        std::size_t pos = 0;
        while ((pos = text.find(token, pos)) != std::string::npos) {
            text.replace(pos, token.size(), value);
            pos += value.size();
        }
    }

    std::string formatDate(const boost::posix_time::ptime& value, const platform::I18n& i18n) {
        std::string format = i18n.get("receipt.date_format");
        if (format == "receipt.date_format")
            format = "%Y-%m-%d %H:%M:%S";
        const std::tm date = boost::posix_time::to_tm(value);
        std::ostringstream out;
        out << std::put_time(&date, format.c_str());
        return out.str();
    }

    std::string linesFor(const std::vector<TicketLine>& lines, const platform::I18n& i18n, const ReceiptLayout& layout) {
        std::ostringstream out;
        for (const auto& line: lines) {
            out << layout.left(line.description) << "\n";
            std::ostringstream detail;
            detail << line.quantity / 1000;
            if (line.quantity % 1000 != 0)
                detail << i18n.get("number.decimal_separator")
                       << (line.quantity % 1000 < 100 ? "0" : "")
                       << (line.quantity % 1000 < 10 ? "0" : "")
                       << line.quantity % 1000;
            detail << i18n.get("number.quantity_separator") << formatMoney(line.unitPrice, i18n);
            out << layout.leftRight(detail.str(), formatMoney(line.total, i18n)) << "\n";
        }
        return out.str();
    }

    std::string apply(std::string text, const Ticket& invoice, const std::vector<TicketLine>& lines, const std::string& businessName, const platform::I18n& i18n, std::string footer, const ReceiptLayout& layout, const TicketTemplate::TemplateData& custom) {
        if (footer.empty())
            footer = i18n.get("receipt.footer");

        NexoBarImp app;

        TicketTemplate::TemplateData data;
        const auto localIssuedAt = AppTime::toLocal(invoice.issuedAt).local_time();
        const auto date = formatDate(localIssuedAt, i18n);
        const auto total = formatMoney(invoice.total, i18n);
        const auto optionalValue = [](const auto& value) { return value.value_or(std::string{}); };
        const auto& business = app->business();
        std::string_view typeKey = "ticket_types.simplified";
        switch (invoice.type) {
            case TicketType::Simplified: typeKey = "ticket_types.simplified"; break;
            case TicketType::Complete: typeKey = "ticket_types.complete"; break;
            case TicketType::RectifyingSimplified: typeKey = "ticket_types.rectifying_simplified"; break;
            case TicketType::RectifyingComplete: typeKey = "ticket_types.rectifying_complete"; break;
        }
        const std::string typeName = i18n.get(typeKey);
        data.values = {
            {"business.name", businessName}, {"business.legal_name", business.legalName},
            {"business.tax_id", business.nif}, {"business.address", business.address},
            {"business.postal_code", business.postalCode}, {"business.city", business.city},
            {"business.province", business.province}, {"business.country", business.country},
            {"business.phone", optionalValue(business.phone)}, {"business.email", optionalValue(business.email)},
            {"ticket.id", std::to_string(invoice.id.get())}, {"ticket.number", invoice.fullNumber},
            {"ticket.series", invoice.seriesCode}, {"ticket.date", date},
            {"ticket.issue_date", boost::gregorian::to_iso_extended_string(localIssuedAt.date())},
            {"ticket.issue_time", boost::posix_time::to_simple_string(localIssuedAt.time_of_day())},
            {"ticket.total", total},
            {"ticket.tax_base", formatMoney(invoice.taxBase, i18n)}, {"ticket.tax_amount", formatMoney(invoice.taxAmount, i18n)},
            {"ticket.customer_name", optionalValue(invoice.customerName)},
            {"ticket.customer_tax_id", optionalValue(invoice.customerTaxNumber)},
            {"ticket.customer_address", optionalValue(invoice.customerAddress)},
            {"ticket.rectification_reason", optionalValue(invoice.rectificationReason)},
            {"ticket.qr", {}}, {"ticket.qr_url", {}},
            {"ticket.currency", i18n.get("number.currency_suffix")}, {"ticket.type", typeName},
            {"fiscal.hash", {}}, {"fiscal.previous_hash", {}}, {"fiscal.generated_at", {}},
            {"fiscal.signature_format", {}}, {"fiscal.system_name", std::string(SoftwareProducerIdentity::SystemName)},
            {"fiscal.system_id", std::string(SoftwareProducerIdentity::SystemId)},
            {"fiscal.software_version", std::string(SoftwareProducerIdentity::Version)},
            {"fiscal.installation_number", business.installationNumber},
            {"fiscal.producer_name", std::string(SoftwareProducerIdentity::LegalName)},
            {"fiscal.producer_nif", std::string(SoftwareProducerIdentity::Nif)},
            {"ticket.footer", layout.center(footer)}, {"receipt.ticket_label", i18n.get("receipt.ticket")},
            {"receipt.date_label", i18n.get("receipt.date")}, {"receipt.total_label", i18n.get("receipt.total")},
            {"separator", layout.separator()}
        };
        if (app->database()) {
            const auto fiscal = app->database()->selectFirst<FiscalRecord>(
                Nearon::col<&FiscalRecord::ticketId> == invoice.id &&
                Nearon::col<&FiscalRecord::type> == FiscalRecordType::Issue);
            if (fiscal) {
                data.values.insert_or_assign("fiscal.hash", fiscal->hash);
                data.values.insert_or_assign("fiscal.previous_hash", fiscal->previousHash.value_or(""));
                data.values.insert_or_assign("fiscal.generated_at", boost::posix_time::to_iso_extended_string(fiscal->generatedAt));
                data.values.insert_or_assign("fiscal.signature_format", fiscal->signatureFormat);
                if (fiscal->qrPayload && (FiscalService::certificateInstalled() || business.qrEnabled)) {
                    data.values.insert_or_assign("ticket.qr", escPosQr(*fiscal->qrPayload));
                    data.values.insert_or_assign("ticket.qr_url", *fiscal->qrPayload);
                }
            }
            auto& taxRows = data.sections["taxes"];
            for (const auto& tax : app->database()->selectWhere<TicketTax>(
                     Nearon::col<&TicketTax::ticketId> == invoice.id)) {
                std::ostringstream rate;
                rate << tax.rate / 100 << i18n.get("number.decimal_separator")
                     << std::setfill('0') << std::setw(2) << std::abs(tax.rate % 100) << '%';
                TicketTemplate::TemplateData row;
                row.values = {{"tax.rate", rate.str()}, {"tax.base", formatMoney(tax.taxBase, i18n)},
                    {"tax.amount", formatMoney(tax.taxAmount, i18n)}, {"tax.code", tax.taxCode},
                    {"tax.regime", tax.regimeCode}, {"tax.operation", tax.operationCode},
                    {"tax.row", layout.leftRight(rate.str() + " " + formatMoney(tax.taxBase, i18n),
                                                  formatMoney(tax.taxAmount, i18n))}};
                taxRows.push_back(std::move(row));
            }
        }
        auto& lineRows = data.sections["lines"];
        lineRows.reserve(lines.size());
        for (const auto& line: lines) {
            std::ostringstream quantity;
            quantity << line.quantity / 1000;
            if (line.quantity % 1000)
                quantity << i18n.get("number.decimal_separator") << std::setw(3)
                         << std::setfill('0') << line.quantity % 1000;
            const auto unit = formatMoney(line.unitPrice, i18n), lineTotal = formatMoney(line.total, i18n);
            TicketTemplate::TemplateData row;
            row.values = {
                {"line.id", std::to_string(line.id.get())}, {"line.number", std::to_string(line.lineNumber)},
                {"line.description", line.description}, {"line.quantity", quantity.str()},
                {"line.unit_price", unit}, {"line.discount", formatMoney(line.discount, i18n)},
                {"line.tax_rate", std::to_string(line.taxRate)}, {"line.total", lineTotal},
                {"line.row", layout.left(line.description) + "\n" +
                             layout.leftRight(quantity.str() + i18n.get("number.quantity_separator") + unit, lineTotal)}
            };
            lineRows.push_back(std::move(row));
        }

        replaceAll(text, "{{BUSINESS_NAME}}", layout.center(businessName));
        replaceAll(text, "{{OWNER}}", layout.leftRight(app->business().legalName, app->business().nif));
        replaceAll(text, "{{BUSINESS_NAME_RAW}}", businessName);
        replaceAll(text, "{{TICKET_ID}}", invoice.fullNumber);
        replaceAll(text, "{{TICKET_LABEL}}", i18n.get("receipt.ticket"));
        replaceAll(text, "{{DATE_LABEL}}", i18n.get("receipt.date"));
        replaceAll(text, "{{TOTAL_LABEL}}", i18n.get("receipt.total"));
        replaceAll(text, "{{DATE}}", date);
        replaceAll(text, "{{DATE_ROW}}", layout.left(i18n.get("receipt.date") + i18n.get("number.label_separator") + date));
        replaceAll(text, "{{TICKET_ROW}}", layout.left(i18n.get("receipt.ticket") + i18n.get("number.label_separator") + invoice.fullNumber));
        replaceAll(text, "{{LINES}}", linesFor(lines, i18n, layout));
        replaceAll(text, "{{TOTAL}}", total);
        replaceAll(text, "{{TOTAL_ROW}}", layout.leftRight(i18n.get("receipt.total"), total));
        replaceAll(text, "{{FOOTER}}", layout.center(footer));
        replaceAll(text, "{{SEPARATOR}}", layout.separator());

        // Generic template directive: {{RIGHT:any text}} places arbitrary content at the final column.
        std::size_t directive = 0;
        while ((directive = text.find("{{RIGHT:", directive)) != std::string::npos) {
            const auto end = text.find("}}", directive + 8);
            if (end == std::string::npos) break;
            const auto value = text.substr(directive + 8, end - directive - 8);
            const auto rendered = layout.right(value);
            text.replace(directive, end + 2 - directive, rendered);
            directive += rendered.size();
        }
        data.values.insert_or_assign("date", date);
        data.values.insert_or_assign("total", total);
        data.values.insert_or_assign("footer", layout.center(footer));
        data.values.insert_or_assign("lines", linesFor(lines, i18n, layout));
        for (const auto& [key,value]: custom.values)data.values.insert_or_assign(key, value);
        for (const auto& [key,rows]: custom.sections)data.sections.insert_or_assign(key, rows);
        return TicketTemplate::render(std::move(text), data, &layout);
    }
}

std::string TicketTemplate::render(std::string source, const TemplateData& data,
                                   const ReceiptLayout* layout) {
    return finishJoinedRows(renderContext(std::move(source), data), layout);
}

std::string TicketTemplate::renderFile(const std::string& path, const Ticket& invoice, const std::vector<TicketLine>& lines, const std::string& businessName, const platform::I18n& i18n, const std::string& footer, const ReceiptLayout& layout, const TemplateData& custom) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error(i18n.get("errors.ticket_template_missing"));
    std::ostringstream content;
    content << file.rdbuf();
    return apply(content.str(), invoice, lines, businessName, i18n, footer, layout, custom);
}

TicketTemplate::PrintResult TicketTemplate::printFile(const std::string& path, const Ticket& invoice, const std::vector<TicketLine>& lines, const std::string& businessName, const platform::I18n& i18n, const std::string& footer, const ReceiptLayout& layout, const TemplateData& custom) {
    PrintResult result;
    result.rendered = renderFile(path, invoice, lines, businessName, i18n, footer, layout, custom);
    NEXO_INFO(LogModule::Core) << "Rendered ticket:\n"
                              << humanReadableEscPos(result.rendered, "[AEAT QR]");
    TicketPrinter output;
    result.printed = output.printTicketAndOpenDrawer(result.rendered, result.error, i18n);
    return result;
}

std::string TicketTemplate::renderDefault(const Ticket& invoice, const std::vector<TicketLine>& lines, const std::string& businessName, const platform::I18n& i18n, const std::string& footer, const ReceiptLayout& layout, const TemplateData& custom) {
    const std::string text = "\x1B" "a" "\x01" "{{BUSINESS_NAME}}\n"
            "{{SEPARATOR}}\n"
            "{{TICKET_ROW}}\n{{DATE_ROW}}\n"
            "{{SEPARATOR}}\n"
            "{{LINES}}"
            "{{SEPARATOR}}\n"
            "{{TOTAL_ROW}}\n\n"
            "\x1B" "a" "\x01" "{{FOOTER}}\n"
            "\x1B" "a" "\x00";
    return apply(text, invoice, lines, businessName, i18n, footer, layout, custom);
}
