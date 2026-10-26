#pragma once

#include "database/hospitality/HospitalityDatabase.h"
#include "ReceiptLayout.h"
#include <string>
#include <vector>

class TicketPrinter {
public:
    TicketPrinter() = default;

    bool printRaw(const std::string& bytes, std::string& error, const platform::I18n& i18n) const;

    bool printText(const std::string& text, std::string& error, const platform::I18n& i18n) const;

    // Sends drawer pulse and the already rendered ESC/POS receipt as one RAW job.
    bool printTicketAndOpenDrawer(const std::string& text, std::string& error, const platform::I18n& i18n) const;

    bool openDrawer(std::string& error, const platform::I18n& i18n) const;

    static std::string makeReceipt(const nexobar::Ticket& invoice,
                                   const std::vector<nexobar::TicketLine>& lines,
                                   const std::string& businessName,
                                   const platform::I18n& i18n,
                                   const std::string& footer = {},
                                   const ReceiptLayout& layout = ReceiptLayout{});

private:
    std::string effectivePrinterName(std::string& error, const platform::I18n& i18n) const;
};
