//
// Created by cadox8 on 23/10/2026.
//
#pragma once
#include "../export.h"

#include <cstdint>
#include <string>

class PLATFORM_API License {
public:
    License() = default;

    ~License() = default;

    enum class LicenseType : uint8_t {
        Basic = 0,
        Standard,
        Pro,
        Enterprise,
        Invalid = 255
    };

    /**
     * Generates the license string.
     *
     * @param type The type of license to generate
     * @return ~24-32 characters license string
     */
    std::string generate(const LicenseType& type);

    LicenseType validate(const std::string& licenseStr) const;

    const std::string typeName(const LicenseType& t);
};
