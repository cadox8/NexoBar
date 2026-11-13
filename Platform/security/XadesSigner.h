#pragma once

#include "../export.h"

#include <filesystem>
#include <string>
#include <string_view>

namespace nexobar {
    struct XadesResult {
        std::string xml;
        std::string signatureXml;
        std::string certificateSubject;
        std::string certificateThumbprint;
    };

    /// XAdES-EPES enveloped signatures following the AEAT SIF profile v0.1.5.
    class PLATFORM_API XadesSigner {
    public:
        static XadesResult sign(std::string_view xml,
                                const std::filesystem::path& pkcs12Path,
                                std::string_view password,
                                std::string_view signingTime);
        /// Uses the operating-system certificate manager. On Windows this is
        /// CurrentUser\\MY; on Linux it is an OpenSSL store URI (PKCS#11/p11-kit).
        static XadesResult signSystem(std::string_view xml,
                                      std::string_view certificateSelector,
                                      std::string_view signingTime);
        static bool verify(std::string_view signedXml, std::string& error);
        static bool validateSchema(std::string_view xml,
                                   const std::filesystem::path& schemaPath,
                                   std::string& error);
    };
}
