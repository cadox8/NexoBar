#pragma once

#include <string_view>

namespace nexobar {
    /// Producer identity used in fiscal exports and declarations.
    /// The public repository uses non-production test values. Replace them and
    /// enable ConfiguredForProduction only in reviewed private builds.
    struct SoftwareProducerIdentity final {
        static constexpr bool ConfiguredForProduction = false;
        static constexpr std::string_view LegalName = "Productor Prueba";
        static constexpr std::string_view Nif = "B12345678";
        static constexpr std::string_view PostalAddress = "Direccion de prueba";
        static constexpr std::string_view City = "Ciudad de prueba";
        static constexpr std::string_view Province = "Provincia de prueba";
        static constexpr std::string_view Country = "ES";
        static constexpr std::string_view Phone = "000000000";
        static constexpr std::string_view Email = "productor@example.test";
        static constexpr std::string_view DocumentSigningCertificateThumbprint = "";
        static constexpr std::string_view CodeSigningCertificateThumbprint = "";

        static constexpr std::string_view SystemName = "NexoBar";
        static constexpr std::string_view SystemId = "NB";
        static constexpr std::string_view Version = "1.0.0";

        [[nodiscard]] static constexpr bool declarationDataComplete() noexcept {
            return ConfiguredForProduction && !LegalName.empty() && !Nif.empty() && !PostalAddress.empty() &&
                   !PostalAddress.starts_with("PENDIENTE") && !City.starts_with("PENDIENTE") &&
                   !Province.starts_with("PENDIENTE") && !Phone.starts_with("PENDIENTE") &&
                   !Email.starts_with("PENDIENTE");
        }
    };
}
