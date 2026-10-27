#include "PrinterDiagnostics.h"
#include "TicketPrinter.h"
#include "i18n/I18n.h"

#include <libusb.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <tuple>

class PrinterProfiles {
public:
    struct Profile { std::uint16_t vendor; const char* manufacturer; const char* commandSet; int defaultPaperMm; };
    static const Profile* find(const std::uint16_t vendor) {
        static constexpr Profile profiles[] = {
            {0x04B8, "Epson", "ESC/POS", 80}, {0x0519, "Star Micronics", "StarPRNT", 80},
            {0x1504, "BIXOLON", "ESC/POS", 80}, {0x2730, "Citizen", "ESC/POS", 80},
            {0x0DD4, "Custom", "ESC/POS", 80}, {0x0FE6, "Rongta", "ESC/POS", 80},
            {0x20D1, "HPRT", "ESC/POS", 80}, {0x1D90, "POSBANK", "ESC/POS", 80},
            {0x154F, "SNBC", "ESC/POS", 80}, {0x1FC9, "DATECS", "ESC/POS", 58},
            {0x0525, "Sewoo/LK", "ESC/POS", 80}, {0x0483, "XPrinter-compatible", "ESC/POS", 80},
            {0x0A5F, "Zebra", "ZPL", 80}, {0x04F9, "Brother", "Brother raster", 62}
        };
        const auto found = std::ranges::find(profiles, vendor, &Profile::vendor);
        return found == std::end(profiles) ? nullptr : &*found;
    }
};

namespace {
    std::string descriptorString(libusb_device_handle* handle, const std::uint8_t index) {
        if (!handle || !index) return {};
        unsigned char text[256]{};
        const int size = libusb_get_string_descriptor_ascii(handle, index, text, sizeof(text));
        return size > 0 ? std::string(reinterpret_cast<char*>(text), static_cast<std::size_t>(size)) : std::string{};
    }
    std::string upper(std::string text) {
        std::ranges::transform(text, text.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        return text;
    }
    int paperWidth(const std::string& identity, const int fallback) {
        const auto value = upper(identity);
        if (value.find("58") != std::string::npos || value.find("2IN") != std::string::npos) return 58;
        if (value.find("76") != std::string::npos || value.find("3IN") != std::string::npos) return 76;
        if (value.find("80") != std::string::npos) return 80;
        if (value.find("112") != std::string::npos || value.find("4IN") != std::string::npos) return 112;
        return fallback;
    }
}

std::vector<PrinterInfo> PrinterDiagnostics::enumerateUsbPrinters(std::string& error,
                                                                  const platform::I18n* i18n) {
    error.clear();
    std::vector<PrinterInfo> result;
    libusb_context* context = nullptr;
    if (const int code = libusb_init(&context); code != 0) {
        error = (i18n ? i18n->get("printer.usb_init_failed") : std::string("printer.usb_init_failed")) +
                libusb_error_name(code);
        return result;
    }
    libusb_device** devices = nullptr;
    const ssize_t count = libusb_get_device_list(context, &devices);
    if (count < 0) {
        error = (i18n ? i18n->get("printer.usb_enumeration_failed") : std::string("printer.usb_enumeration_failed")) +
                libusb_error_name(static_cast<int>(count));
        libusb_exit(context);
        return result;
    }
    for (ssize_t i = 0; i < count; ++i) {
        libusb_device* device = devices[i];
        libusb_device_descriptor descriptor{};
        if (libusb_get_device_descriptor(device, &descriptor) != 0) continue;
        libusb_config_descriptor* configuration = nullptr;
        if (libusb_get_active_config_descriptor(device, &configuration) != 0 &&
            libusb_get_config_descriptor(device, 0, &configuration) != 0) continue;
        const PrinterProfiles::Profile* profile = PrinterProfiles::find(descriptor.idVendor);
        for (std::uint8_t interfaceIndex = 0; interfaceIndex < configuration->bNumInterfaces; ++interfaceIndex) {
            const libusb_interface& interface = configuration->interface[interfaceIndex];
            for (int alternateIndex = 0; alternateIndex < interface.num_altsetting; ++alternateIndex) {
                const libusb_interface_descriptor& alternate = interface.altsetting[alternateIndex];
                std::uint8_t endpoint = 0;
                for (std::uint8_t endpointIndex = 0; endpointIndex < alternate.bNumEndpoints; ++endpointIndex) {
                    const auto& candidate = alternate.endpoint[endpointIndex];
                    if ((candidate.bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_OUT &&
                        (candidate.bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) == LIBUSB_TRANSFER_TYPE_BULK) {
                        endpoint = candidate.bEndpointAddress; break;
                    }
                }
                const bool printerClass = alternate.bInterfaceClass == LIBUSB_CLASS_PRINTER;
                if (!endpoint || (!printerClass && !profile)) continue;
                libusb_device_handle* handle = nullptr;
                libusb_open(device, &handle);
                PrinterInfo info;
                info.vendorId = descriptor.idVendor; info.productId = descriptor.idProduct;
                info.bus = libusb_get_bus_number(device); info.address = libusb_get_device_address(device);
                info.interfaceNumber = alternate.bInterfaceNumber; info.outputEndpoint = endpoint;
                info.manufacturer = descriptorString(handle, descriptor.iManufacturer);
                info.model = descriptorString(handle, descriptor.iProduct);
                info.serial = descriptorString(handle, descriptor.iSerialNumber);
                if (handle) libusb_close(handle);
                if (info.manufacturer.empty() && profile) info.manufacturer = profile->manufacturer;
                std::ostringstream fallback;
                const std::string genericUsb = i18n ? i18n->get("printer.generic_usb") : "USB";
                const std::string genericPrinter = i18n ? i18n->get("printer.generic_device") : "printer";
                fallback << (info.manufacturer.empty() ? genericUsb : info.manufacturer) << ' '
                         << (info.model.empty() ? genericPrinter : info.model) << " ["
                         << std::hex << std::setw(4) << std::setfill('0') << info.vendorId << ':'
                         << std::setw(4) << info.productId << ']';
                info.name = fallback.str();
                info.commandSet = profile ? profile->commandSet
                                          : (i18n ? i18n->get("printer.generic_command_set")
                                                  : "ESC/POS");
                info.paperWidthMm = paperWidth(info.name, profile ? profile->defaultPaperMm : 80);
                info.dpi = 203;
                result.push_back(std::move(info));
                goto next_device;
            }
        }
        next_device:
        libusb_free_config_descriptor(configuration);
    }
    libusb_free_device_list(devices, 1);
    libusb_exit(context);
    std::ranges::sort(result, [](const PrinterInfo& a, const PrinterInfo& b) {
        return std::tie(a.bus, a.address) < std::tie(b.bus, b.address);
    });
    return result;
}

std::optional<PrinterInfo> PrinterDiagnostics::selectedUsbPrinter(std::string& error, const platform::I18n* i18n) {
    const auto printers = enumerateUsbPrinters(error, i18n);
    if (printers.empty()) {
        if (error.empty())
            error = i18n ? i18n->get("printer.usb_not_found") : "printer.usb_not_found";
        return std::nullopt;
    }
    if (const char* preferred = std::getenv("NEXOBAR_USB_PRINTER"); preferred && *preferred) {
        const std::string choice(preferred);
        for (const auto& printer : printers) if (printer.name.find(choice) != std::string::npos) return printer;
    }
    return printers.front();
}

PrinterCommands PrinterDiagnostics::commandsFor(const PrinterInfo& printer) {
    if (printer.commandSet == "StarPRNT")
        return {std::string("\x1B@", 2), std::string("\x1B\x64\x03\x1D\x56\x01", 7), std::string("\x07", 1)};
    if (printer.commandSet == "ZPL") return {"^XA", "^XZ", {}};
    if (printer.commandSet == "Brother raster") return {{}, {}, {}};
    return {std::string("\x1B@", 2), std::string("\x1D\x56\x41\x00", 4), std::string("\x1B\x70\x00\x19\xFA", 5)};
}
ReceiptLayout PrinterDiagnostics::receiptLayoutFor(const PrinterInfo& printer) {
    const bool compact = printer.paperWidthMm <= 60;
    return ReceiptLayout(compact ? 32 : printer.paperWidthMm >= 100 ? 64 : 48, 2, 2);
}
ReceiptLayout PrinterDiagnostics::detectedReceiptLayout() {
    std::string error; const auto printer = selectedUsbPrinter(error);
    return printer ? receiptLayoutFor(*printer) : ReceiptLayout{};
}
bool PrinterDiagnostics::testPrinterAndDrawer(std::string& error, const platform::I18n& i18n) {
    return TicketPrinter{}.printTicketAndOpenDrawer(i18n.get("printer.test_body"), error, i18n);
}
bool PrinterDiagnostics::testPrinter(std::string& error, const platform::I18n& i18n) {
    return TicketPrinter{}.printText(i18n.get("printer.test_body"), error, i18n);
}
bool PrinterDiagnostics::testDrawer(std::string& error, const platform::I18n& i18n) {
    return TicketPrinter{}.openDrawer(error, i18n);
}
