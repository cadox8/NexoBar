#pragma once

#include <array>
#include <cstdint>
#include <iomanip>
#include <random>
#include <sstream>
#include <string>
#include <string_view>

namespace nexobar {
    class Pin final {
    public:
        static bool valid(const std::string_view pin) noexcept {
            if (pin.size() < 4 || pin.size() > 6) return false;
            for (const char value : pin) if (value < '0' || value > '9') return false;
            return true;
        }

        static std::string newSalt() {
            std::random_device random;
            std::ostringstream out;
            out << std::hex << std::setfill('0');
            for (int i = 0; i < 4; ++i) out << std::setw(8) << random();
            return out.str();
        }

        static std::string hash(const std::string_view pin, const std::string_view salt) {
            // A deliberately expensive, portable salted derivation. The database
            // never stores the original PIN.
            std::uint64_t left = 0xcbf29ce484222325ULL;
            std::uint64_t right = 0x9e3779b97f4a7c15ULL;
            const std::string input = std::string(salt) + ':' + std::string(pin);
            for (int round = 0; round < 120000; ++round) {
                for (const unsigned char byte : input) {
                    left = (left ^ byte ^ static_cast<std::uint64_t>(round)) * 0x100000001b3ULL;
                    right ^= left + byte + (right << 6) + (right >> 2);
                }
                left ^= right + (left << 13);
                right ^= left + (right >> 7);
            }
            std::ostringstream out;
            out << std::hex << std::setfill('0') << std::setw(16) << left << std::setw(16) << right;
            return out.str();
        }

        static bool matches(const std::string_view pin, const std::string_view salt,
                            const std::string_view expected) {
            const auto actual = hash(pin, salt);
            if (actual.size() != expected.size()) return false;
            unsigned char difference = 0;
            for (std::size_t i = 0; i < actual.size(); ++i)
                difference |= static_cast<unsigned char>(actual[i] ^ expected[i]);
            return difference == 0;
        }
    };
}
