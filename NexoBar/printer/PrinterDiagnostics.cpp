#include "PrinterDiagnostics.h"
#include "TicketPrinter.h"
#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <utility>
#include <algorithm>
#include <cctype>
#ifndef _WIN32
#include <sys/wait.h>
#endif
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <winspool.h>
#endif

namespace {
    std::string upperAscii(std::string value) {
        std::ranges::transform(value, value.begin(), [](const unsigned char c) { return static_cast<char>(std::toupper(c)); });
        return value;
    }

#ifdef _WIN32
    std::string wideToUtf8(const wchar_t* value) {
        if (!value) return {};
        const int count = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
        if (count <= 1) return {};
        std::string result(static_cast<std::size_t>(count), '\0');
        WideCharToMultiByte(CP_UTF8, 0, value, -1, result.data(), count, nullptr, nullptr);
        result.pop_back();
        return result;
    }

#else
    std::string shellQuote(const std::string& value) {
        std::string result = "'";
        for (const char ch: value) {
            if (ch == '\'') result += "'\\''";
            else result.push_back(ch);
        }
        result.push_back('\'');
        return result;
    }

    bool commandSucceeded(const int status) {
        return status != -1 && WIFEXITED(status) && WEXITSTATUS(status) == 0;
    }
#endif
}

std::vector<PrinterInfo> PrinterDiagnostics::enumerateUsbPrinters(std::string& error) {
    error.clear();
    std::vector<PrinterInfo> printers;
#ifdef _WIN32
    DWORD needed = 0, count = 0;
    EnumPrintersW(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS, nullptr, 2, nullptr, 0, &needed, &count);
    const DWORD firstError = GetLastError();
    if (needed == 0 && firstError == ERROR_SUCCESS)return printers;
    if (firstError != ERROR_INSUFFICIENT_BUFFER || needed == 0) {
        error = "EnumPrintersW failed: " + std::to_string(firstError);
        return printers;
    }
    std::vector<BYTE> buffer(needed);
    if (!EnumPrintersW(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS, nullptr, 2, buffer.data(), needed, &needed, &count)) {
        error = "EnumPrintersW failed: " + std::to_string(GetLastError());
        return printers;
    }
    DWORD defaultBytes = 0;GetDefaultPrinterW(nullptr, &defaultBytes);
    std::wstring defaultWide(defaultBytes ? defaultBytes : 1, L'\0');
    if (defaultBytes && GetDefaultPrinterW(defaultWide.data(), &defaultBytes)) {
        if (!defaultWide.empty() && defaultWide.back() == L'\0')defaultWide.pop_back();
    } else defaultWide.clear();
    const auto* entries = reinterpret_cast<const PRINTER_INFO_2W*>(buffer.data());
    for (DWORD i = 0; i < count; ++i) {
        const std::string name = wideToUtf8(entries[i].pPrinterName);
        const std::wstring port = entries[i].pPortName ? entries[i].pPortName : L"";
        // Windows queues backed by a physical USB port only. Excludes FILE:, OneNote, PDF, network queues, etc.
        if (!name.empty() && port.starts_with(L"USB")) {
            PrinterInfo info;
            info.name = name;
            info.isDefault = entries[i].pPrinterName && defaultWide == entries[i].pPrinterName;
            info.driver = wideToUtf8(entries[i].pDriverName);
            info.port = wideToUtf8(entries[i].pPortName);
            if (entries[i].pDevMode) {
                if (entries[i].pDevMode->dmFields & DM_PAPERWIDTH) info.paperWidthMm = entries[i].pDevMode->dmPaperWidth / 10;
                if (entries[i].pDevMode->dmFields & DM_PAPERLENGTH) info.paperHeightMm = entries[i].pDevMode->dmPaperLength / 10;
                if (entries[i].pDevMode->dmFields & DM_PRINTQUALITY && entries[i].pDevMode->dmPrintQuality > 0) info.dpi = entries[i].pDevMode->dmPrintQuality;
            }
            const std::string identity = upperAscii(info.name + " " + info.driver);
            info.commandSet = identity.find("STAR") != std::string::npos ? "Star Line Mode" : "ESC/POS";
            printers.push_back(std::move(info));
        }
    }
#else
    FILE* pipe = popen("lpstat -p -d 2>/dev/null", "r");
    if (!pipe) {
        error = "Could not run lpstat (CUPS tools are required).";
        return printers;
    }
    std::string defaultName, line;
    char buffer[1024];
    while (std::fgets(buffer, sizeof(buffer), pipe)) {
        line = buffer;
        constexpr std::string_view prefix = "printer ";
        if (line.starts_with(prefix)) {
            const auto end = line.find(' ', prefix.size());
            const std::string name = line.substr(prefix.size(), end == std::string::npos ? std::string::npos : end - prefix.size());
            if (!name.empty())printers.push_back({name, false});
        } else {
            constexpr std::string_view defaultPrefix = "system default destination: ";
            const auto pos = line.find(defaultPrefix);
            if (pos != std::string::npos) {
                defaultName = line.substr(pos + defaultPrefix.size());
                while (!defaultName.empty() && (defaultName.back() == '\n' || defaultName.back() == '\r'))defaultName.pop_back();
            }
        }
    }
    const int status = pclose(pipe);
    if (!commandSucceeded(status)) {
        error = "Could not enumerate printers with lpstat (is CUPS installed and running?).";
        return {};
    }
    auto allPrinters = std::move(printers);
    printers.clear();
    for (auto& printer: allPrinters) {
        const std::string command = "lpstat -v " + shellQuote(printer.name) + " 2>/dev/null";
        FILE* devicePipe = popen(command.c_str(), "r");
        if (!devicePipe)continue;
        char deviceBuffer[1024]{};
        std::string deviceLine;
        if (std::fgets(deviceBuffer, sizeof(deviceBuffer), devicePipe))deviceLine = deviceBuffer;
        pclose(devicePipe);
        const auto colon = deviceLine.find(':');
        const std::string uri = colon == std::string::npos ? std::string{} : deviceLine.substr(colon + 1);
        if (uri.find("usb://") != std::string::npos || uri.find("usb:/") != std::string::npos) {
            printer.isDefault = printer.name == defaultName;
            printer.port = uri;
            const std::string identity = upperAscii(printer.name + " " + printer.driver);
            printer.commandSet = identity.find("STAR") != std::string::npos ? "Star Line Mode" : "ESC/POS";
            printers.push_back(std::move(printer));
        }
    }
#endif
    return printers;
}

std::optional<PrinterInfo> PrinterDiagnostics::selectedUsbPrinter(std::string& error) {
    const auto printers = enumerateUsbPrinters(error);
    for (const auto& printer: printers)if (printer.isDefault)return printer;
    if (printers.size() == 1)return printers.front();
    if (error.empty())error = "No default USB receipt printer is configured.";
    return std::nullopt;
}

std::string PrinterDiagnostics::defaultUsbPrinterName(std::string& error) {
    const auto printer = selectedUsbPrinter(error);
    return printer ? printer->name : std::string{};
}

PrinterCommands PrinterDiagnostics::commandsFor(const PrinterInfo& printer) {
    if (printer.commandSet == "Star Line Mode")
        return {std::string("\x1B@", 2), std::string("\x1B\x64\x03\x1D\x56\x01", 7), std::string("\x07", 1)};
    return {std::string("\x1B@", 2), std::string("\x1D\x56\x41\x00", 4), std::string("\x1B\x70\x00\x19\xFA", 5)};
}

ReceiptLayout PrinterDiagnostics::receiptLayoutFor(const PrinterInfo& printer) {
    const std::string identity = upperAscii(printer.name + " " + printer.driver);
    const bool compact = printer.paperWidthMm > 0 ? printer.paperWidthMm <= 60 : identity.find("58") != std::string::npos;
    return ReceiptLayout(compact ? 32 : 48, 2, 2);
}

ReceiptLayout PrinterDiagnostics::detectedReceiptLayout() {
    std::string error;
    const auto printer = selectedUsbPrinter(error);
    return printer ? receiptLayoutFor(*printer) : ReceiptLayout{};
}


bool PrinterDiagnostics::testPrinterAndDrawer(std::string& error, const platform::I18n& i18n) {
    TicketPrinter printer;
    return printer.printTicketAndOpenDrawer("PRUEBA DE IMPRESORA\nCajon conectado\n", error, i18n);
}

bool PrinterDiagnostics::testPrinter(std::string& error, const platform::I18n& i18n) {
    TicketPrinter printer;
    return printer.printText("PRUEBA DE IMPRESORA\nTicket de prueba\n", error, i18n);
}

bool PrinterDiagnostics::testDrawer(std::string& error, const platform::I18n& i18n) {
    TicketPrinter printer;
    return printer.openDrawer(error, i18n);
}

