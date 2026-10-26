#include "TicketPrinter.h"
#include "PrinterDiagnostics.h"
#include "database/hospitality/Money.h"
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <cstdio>
#include <cstdlib>
#ifndef _WIN32
#include <sys/wait.h>
#endif
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <winspool.h>
#else
#include <cwchar>
#endif


namespace {
#ifdef _WIN32
    std::wstring utf8ToWide(const std::string& value) {
        if (value.empty())return {};
        const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
        if (count <= 0)return {};
        std::wstring result(static_cast<std::size_t>(count), L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(), count);
        return result;
    }
#else
    bool commandSucceeded(const int status) {
        return status != -1 && WIFEXITED(status) && WEXITSTATUS(status) == 0;
    }
#endif
}

std::string TicketPrinter::effectivePrinterName(std::string& error, const platform::I18n& i18n) const {
    const auto name = PrinterDiagnostics::defaultUsbPrinterName(error);
    if (name.empty() && error.empty())error = i18n.get("printer.name_missing");
    return name;
}

bool TicketPrinter::printRaw(const std::string& bytes, std::string& error, const platform::I18n& i18n) const {
    const std::string selectedName = effectivePrinterName(error, i18n);
    if (selectedName.empty())return false;
#ifdef _WIN32
    const std::wstring wideName = utf8ToWide(selectedName);
    if (wideName.empty()) {
        error = "Printer name is not valid UTF-8.";
        return false;
    }
    HANDLE printer = nullptr;
    if (!OpenPrinterW(const_cast<wchar_t*>(wideName.c_str()), &printer, nullptr)) {
        error = i18n.get("printer.open_failed") + std::to_string(GetLastError());
        return false;
    }
    DOC_INFO_1W doc{};
    const std::string jobName = i18n.get("printer.job_name");
    const std::wstring wideJobName = utf8ToWide(jobName);
    doc.pDocName = const_cast<wchar_t*>(wideJobName.c_str());
    doc.pOutputFile = nullptr;
    doc.pDatatype = const_cast<wchar_t*>(L"RAW");
    const DWORD job = StartDocPrinterW(printer, 1, reinterpret_cast<LPBYTE>(&doc));
    if (!job) {
        error = i18n.get("printer.start_failed") + std::to_string(GetLastError());
        ClosePrinter(printer);
        return false;
    }
    bool ok = StartPagePrinter(printer) != 0;
    DWORD written = 0;
    if (ok && !bytes.empty()) ok = WritePrinter(printer, const_cast<char*>(bytes.data()), static_cast<DWORD>(bytes.size()), &written) != 0 && written == bytes.size();
    if (ok) ok = EndPagePrinter(printer) != 0;
    EndDocPrinter(printer);
    if (!ok) error = i18n.get("printer.write_failed") + std::to_string(GetLastError());
    ClosePrinter(printer);
    return ok;
#else
    const std::string command = "lp -d " + shellQuote(selectedName) + " -o raw 2>/dev/null";
    FILE* pipe = popen(command.c_str(), "w");
    if (!pipe) {
        error = "Could not start the CUPS lp command.";
        return false;
    }
    const std::size_t written = bytes.empty() ? 0 : std::fwrite(bytes.data(), 1, bytes.size(), pipe);
    const int status = pclose(pipe);
    if (written != bytes.size() || !commandSucceeded(status)) {
        error = "CUPS failed to submit the raw print job.";
        return false;
    }
    error.clear();
    return true;
#endif
}

bool TicketPrinter::printText(const std::string& text, std::string& error, const platform::I18n& i18n) const {
    const auto selected = PrinterDiagnostics::selectedUsbPrinter(error);
    if (!selected)return false;
    const auto commands = PrinterDiagnostics::commandsFor(*selected);
    std::string data = commands.initialize;
    data += text;
    data += "\n\n\n" + commands.cut;
    return printRaw(data, error, i18n);
}

bool TicketPrinter::printTicketAndOpenDrawer(const std::string& text, std::string& error, const platform::I18n& i18n) const {
    const auto selected = PrinterDiagnostics::selectedUsbPrinter(error);
    if (!selected)return false;
    const auto commands = PrinterDiagnostics::commandsFor(*selected);
    std::string data = commands.initialize;
    data += commands.drawer;
    data += text;
    // Receipt template owns its content; add feed/cut after its body.
    data += "\n\n\n" + commands.cut;
    return printRaw(data, error, i18n);
}

bool TicketPrinter::openDrawer(std::string& error, const platform::I18n& i18n) const {
    const auto selected = PrinterDiagnostics::selectedUsbPrinter(error);
    if (!selected)return false;
    return printRaw(PrinterDiagnostics::commandsFor(*selected).drawer, error, i18n);
}

std::string TicketPrinter::makeReceipt(const nexobar::Ticket& invoice,
                                       const std::vector<nexobar::TicketLine>& lines,
                                       const std::string& businessName, const platform::I18n& i18n,
                                       const std::string& footer, const ReceiptLayout& layout) {
    const std::string localizedFooter = footer.empty() ? i18n.get("receipt.footer") : footer;
    std::ostringstream out;
    out << '\x1B' << 'a' << '\x00' << layout.center(businessName) << "\n";
    out << layout.separator() << "\n";
    out << layout.leftRight(i18n.get("receipt.ticket") + ": " + invoice.fullNumber, {}, 0) << "\n";
    out << layout.leftRight(i18n.get("receipt.date") + ": " + boost::posix_time::to_simple_string(invoice.issuedAt), {}, 0) << "\n";
    out << layout.separator() << "\n";
    out << layout.row({
        {i18n.get("receipt.item"), layout.contentColumns() - std::min<std::size_t>(12, layout.contentColumns()), ReceiptLayout::Alignment::Left},
        {i18n.get("receipt.amount"), std::min<std::size_t>(12, layout.contentColumns()), ReceiptLayout::Alignment::Right}
    }) << "\n";
    for (const auto& line: lines) {
        out << layout.left(line.description) << "\n";
        std::ostringstream detail;
        detail << line.quantity / 1000;
        if (line.quantity % 1000 != 0) detail << "." << std::setw(3) << std::setfill('0') << line.quantity % 1000 << std::setfill(' ');
        detail << " x " << nexobar::formatMoney(line.unitPrice, i18n);
        out << layout.leftRight(detail.str(), nexobar::formatMoney(line.total, i18n)) << "\n";
    }
    out << layout.separator() << "\n";
    out << layout.leftRight(i18n.get("receipt.total"), nexobar::formatMoney(invoice.total, i18n)) << "\n\n";
    out << layout.center(localizedFooter) << "\n";
    return out.str();
}
