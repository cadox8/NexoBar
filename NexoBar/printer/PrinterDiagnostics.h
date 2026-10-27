#pragma once

#include "i18n/I18n.h"

#include <string>
#include <optional>
#include <vector>
#include <cstdint>

#include "ReceiptLayout.h"

namespace platform {
    class I18n;
}

struct PrinterInfo {
    std::string name;
    std::string manufacturer;
    std::string model;
    std::string serial;
    std::uint16_t vendorId = 0;
    std::uint16_t productId = 0;
    std::uint8_t bus = 0;
    std::uint8_t address = 0;
    std::uint8_t interfaceNumber = 0;
    std::uint8_t outputEndpoint = 0;
    int paperWidthMm = 0;
    int paperHeightMm = 0;
    int dpi = 0;
    std::string commandSet;
};

struct PrinterCommands {
    std::string initialize;
    std::string cut;
    std::string drawer;
};

// Device discovery and explicit hardware checks live outside the print transport.
class PrinterDiagnostics {
public:
    static std::vector<PrinterInfo> enumerateUsbPrinters(std::string& error,
                                                          const platform::I18n* i18n = nullptr);

    static std::optional<PrinterInfo> selectedUsbPrinter(std::string& error, const platform::I18n* i18n = nullptr);

    static PrinterCommands commandsFor(const PrinterInfo& printer);

    static ReceiptLayout receiptLayoutFor(const PrinterInfo& printer);

    static ReceiptLayout detectedReceiptLayout();

    static bool testPrinterAndDrawer(std::string& error, const platform::I18n& i18n);

    static bool testPrinter(std::string& error, const platform::I18n& i18n);

    static bool testDrawer(std::string& error, const platform::I18n& i18n);
};
