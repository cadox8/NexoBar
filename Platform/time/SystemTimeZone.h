#pragma once

#include <string>

#include "../export.h"

class PLATFORM_API SystemTimeZone {
public:
    static std::string getPOSIXTimeZone();
};
