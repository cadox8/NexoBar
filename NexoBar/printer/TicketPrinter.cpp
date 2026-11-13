#include "TicketPrinter.h"
#include "PrinterDiagnostics.h"
#include "UsbPrinterTransport.h"
#include "database/hospitality/Money.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

bool TicketPrinter::printRaw(const std::string& bytes, std::string& error, const platform::I18n& i18n) const {
    const auto printer = PrinterDiagnostics::selectedUsbPrinter(error, &i18n);
    if (!printer) return false;
    return UsbPrinterTransport{}.write(*printer, bytes, error, i18n);
}
bool TicketPrinter::printText(const std::string& text, std::string& error, const platform::I18n& i18n) const {
    const auto printer = PrinterDiagnostics::selectedUsbPrinter(error, &i18n);
    if (!printer) return false;
    const auto commands = PrinterDiagnostics::commandsFor(*printer);
    return UsbPrinterTransport{}.write(*printer, commands.initialize + text + "\n\n\n" + commands.cut, error, i18n);
}
bool TicketPrinter::printTicketAndOpenDrawer(const std::string& text, std::string& error, const platform::I18n& i18n) const {
    const auto printer = PrinterDiagnostics::selectedUsbPrinter(error, &i18n);
    if (!printer) return false;
    const auto commands = PrinterDiagnostics::commandsFor(*printer);
    return UsbPrinterTransport{}.write(*printer, commands.initialize + commands.drawer + text + "\n\n\n" + commands.cut, error, i18n);
}
bool TicketPrinter::openDrawer(std::string& error, const platform::I18n& i18n) const {
    const auto printer = PrinterDiagnostics::selectedUsbPrinter(error, &i18n);
    if (!printer) return false;
    const auto commands = PrinterDiagnostics::commandsFor(*printer);
    if (commands.drawer.empty()) { error = i18n.get("printer.drawer_unsupported"); return false; }
    return UsbPrinterTransport{}.write(*printer, commands.drawer, error, i18n);
}

std::string TicketPrinter::makeReceipt(const nexobar::Ticket& invoice,
                                       const std::vector<nexobar::TicketLine>& lines,
                                       const std::string& businessName, const platform::I18n& i18n,
                                       const std::string& footer, const ReceiptLayout& layout) {
    const std::string localizedFooter = footer.empty() ? i18n.get("receipt.footer") : footer;
    std::ostringstream out;
    out << '\x1B' << 'a' << '\x00' << layout.center(businessName) << "\n";
    out << layout.separator() << "\n";
    out << layout.leftRight(i18n.get("receipt.ticket") + i18n.get("number.label_separator") + invoice.fullNumber, {}, 0) << "\n";
    out << layout.leftRight(i18n.get("receipt.date") + i18n.get("number.label_separator") + boost::posix_time::to_simple_string(invoice.issuedAt), {}, 0) << "\n";
    out << layout.separator() << "\n";
    out << layout.row({
        {i18n.get("receipt.item"), layout.contentColumns() - std::min<std::size_t>(12, layout.contentColumns()), ReceiptLayout::Alignment::Left},
        {i18n.get("receipt.amount"), std::min<std::size_t>(12, layout.contentColumns()), ReceiptLayout::Alignment::Right}
    }) << "\n";
    for (const auto& line : lines) {
        out << layout.left(line.description) << "\n";
        std::ostringstream detail;
        detail << line.quantity / 1000;
        if (line.quantity % 1000 != 0)
            detail << i18n.get("number.decimal_separator") << std::setw(3)
                   << std::setfill('0') << line.quantity % 1000 << std::setfill(' ');
        detail << i18n.get("number.quantity_separator") << nexobar::formatMoney(line.unitPrice, i18n);
        out << layout.leftRight(detail.str(), nexobar::formatMoney(line.total, i18n)) << "\n";
    }
    out << layout.separator() << "\n";
    out << layout.leftRight(i18n.get("receipt.total"), nexobar::formatMoney(invoice.total, i18n)) << "\n\n";
    out << layout.center(localizedFooter) << "\n";
    return out.str();
}
