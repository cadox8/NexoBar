#pragma once

#include <algorithm>
#include <string>
#include <string_view>

inline std::string humanReadableEscPos(const std::string_view bytes,
                                       const std::string_view qrReplacement) {
    std::string result;
    for (std::size_t i = 0; i < bytes.size();) {
        const auto byte = static_cast<unsigned char>(bytes[i]);
        if (byte == 0x1B && i + 1 < bytes.size()) {
            const auto command = static_cast<unsigned char>(bytes[i + 1]);
            i += command == 'p' ? std::min<std::size_t>(5, bytes.size() - i)
                                : command == 'a' ? std::min<std::size_t>(3, bytes.size() - i)
                                                 : std::min<std::size_t>(2, bytes.size() - i);
            continue;
        }
        if (byte == 0x1D && i + 1 < bytes.size()) {
            if (i + 4 < bytes.size() && bytes[i + 1] == '(' && bytes[i + 2] == 'k') {
                const std::size_t payloadLength = static_cast<unsigned char>(bytes[i + 3]) |
                                                  static_cast<std::size_t>(static_cast<unsigned char>(bytes[i + 4])) << 8;
                if (i + 6 < bytes.size() && static_cast<unsigned char>(bytes[i + 5]) == 0x31 &&
                    static_cast<unsigned char>(bytes[i + 6]) == 0x51 && !qrReplacement.empty()) {
                    if (!result.empty() && result.back() != '\n') result += '\n';
                    result += qrReplacement;
                    result += '\n';
                }
                i += std::min<std::size_t>(5 + payloadLength, bytes.size() - i);
            } else {
                i += std::min<std::size_t>(bytes[i + 1] == 'V' ? 3 : 2, bytes.size() - i);
            }
            continue;
        }
        if (byte == '\n' || byte == '\t' || byte >= 0x20) result.push_back(bytes[i]);
        ++i;
    }
    return result;
}
