#include "UsbPrinterTransport.h"

#include <libusb.h>
#include "i18n/I18n.h"

#include <algorithm>

bool UsbPrinterTransport::write(const PrinterInfo& printer, const std::string& bytes, std::string& error, const platform::I18n& i18n) const {
    libusb_context* context = nullptr;
    if (const int code = libusb_init(&context); code != 0) {
        error = i18n.get("printer.usb_init_failed") + libusb_error_name(code);
        return false;
    }
    libusb_device_handle* handle = nullptr;
    libusb_device** devices = nullptr;
    const ssize_t count = libusb_get_device_list(context, &devices);
    for (ssize_t i = 0; i < count && !handle; ++i) {
        libusb_device* device = devices[i];
        libusb_device_descriptor descriptor{};
        if (libusb_get_device_descriptor(device, &descriptor) != 0) continue;
        if (descriptor.idVendor == printer.vendorId && descriptor.idProduct == printer.productId &&
            libusb_get_bus_number(device) == printer.bus && libusb_get_device_address(device) == printer.address)
            libusb_open(device, &handle);
    }
    libusb_free_device_list(devices, 1);
    if (!handle) { error = i18n.get("printer.usb_open_failed"); libusb_exit(context); return false; }
#ifndef _WIN32
    if (libusb_kernel_driver_active(handle, printer.interfaceNumber) == 1)
        libusb_detach_kernel_driver(handle, printer.interfaceNumber);
#endif
    int code = libusb_claim_interface(handle, printer.interfaceNumber);
    if (code != 0) {
        error = i18n.get("printer.usb_claim_failed") + libusb_error_name(code);
        libusb_close(handle); libusb_exit(context); return false;
    }
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const int chunk = static_cast<int>(std::min<std::size_t>(bytes.size() - offset, 16 * 1024));
        int transferred = 0;
        code = libusb_bulk_transfer(handle, printer.outputEndpoint,
            reinterpret_cast<unsigned char*>(const_cast<char*>(bytes.data() + offset)), chunk, &transferred, 5000);
        if (code != 0 || transferred != chunk) {
            error = i18n.get("printer.usb_write_failed") + libusb_error_name(code);
            break;
        }
        offset += static_cast<std::size_t>(transferred);
    }
    libusb_release_interface(handle, printer.interfaceNumber);
    libusb_close(handle);
    libusb_exit(context);
    return offset == bytes.size();
}
