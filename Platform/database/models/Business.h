#pragma once

#include "../hospitality/Types.h"
#include "../Orm.h"

#include <optional>
#include <string>

namespace nexobar {
    class Business {
    public:
        BusinessId id{};
        std::string legalName;
        std::string tradeName;
        std::string nif;
        std::string address;
        std::string postalCode;
        std::string city;
        std::string province;
        std::string country{"ES"};
        std::optional<std::string> phone;
        std::optional<std::string> email;
        std::string license;
        std::string installationNumber;
        bool qrEnabled{true};
    };

    using BusinessTable = Nearon::Table<"business", Business,
        Nearon::Column < "id", &Business::id, Nearon::PrimaryKey>
    ,
    Nearon::Column<"legal_name", &Business::legalName>
    ,
    Nearon::Column<"trade_name", &Business::tradeName>
    ,
    Nearon::Column<"nif", &Business::nif>
    ,
    Nearon::Column<"address", &Business::address>
    ,
    Nearon::Column<"postal_code", &Business::postalCode>
    ,
    Nearon::Column<"city", &Business::city>
    ,
    Nearon::Column<"province", &Business::province>
    ,
    Nearon::Column<"country", &Business::country, Nearon::Default<"'ES'"> >
    ,
    Nearon::Column<"phone", &Business::phone>
    ,
    Nearon::Column<"email", &Business::email>
    ,
    Nearon::Column<"license", &Business::license>
    ,
    Nearon::Column<"installation_number", &Business::installationNumber, Nearon::Default<"''">>
    ,
    Nearon::Column<"qr_enabled", &Business::qrEnabled, Nearon::Default<"1">>
    >;

    NEARON_TABLE(BusinessTable);
} // namespace nexobar
