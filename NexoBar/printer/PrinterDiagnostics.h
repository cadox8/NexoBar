#pragma once

#include <string>
#include <optional>
#include <vector>

#include "ReceiptLayout.h"

namespace platform {
    class I18n;
}

struct PrinterInfo {
    std::string name;
    bool isDefault = false;
    std::string driver;
    std::string port;
    int paperWidthMm = 0;
    int paperHeightMm = 0;
    int dpi = 0;
    std::string commandSet = "ESC/POS";
};

struct PrinterCommands {
    std::string initialize;
    std::string cut;
    std::string drawer;
};

// Device discovery and explicit hardware checks live outside the print transport.
class PrinterDiagnostics {
public:
    static std::vector<PrinterInfo> enumerateUsbPrinters(std::string& error);

    static std::optional<PrinterInfo> selectedUsbPrinter(std::string& error);

    static std::string defaultUsbPrinterName(std::string& error);

    static PrinterCommands commandsFor(const PrinterInfo& printer);

    static ReceiptLayout receiptLayoutFor(const PrinterInfo& printer);

    static ReceiptLayout detectedReceiptLayout();

    static bool testPrinterAndDrawer(std::string& error, const platform::I18n& i18n);

    static bool testPrinter(std::string& error, const platform::I18n& i18n);

    static bool testDrawer(std::string& error, const platform::I18n& i18n);
};
