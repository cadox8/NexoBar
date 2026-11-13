#pragma once

#include "../export.h"

#include <string>
#include <string_view>

namespace nexobar {
    /// SHA-256 encoded as 64 upper-case hexadecimal characters.
    PLATFORM_API std::string sha256(std::string_view input);
}
