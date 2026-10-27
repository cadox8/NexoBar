#pragma once

#include "i18n/I18n.h"

#include "PrinterDiagnostics.h"

namespace platform { class I18n; }

#include <string>

class UsbPrinterTransport {
public:
    bool write(const PrinterInfo& printer, const std::string& bytes, std::string& error, const platform::I18n& i18n) const;
};
