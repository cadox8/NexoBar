#pragma once

#include <string>
#include <vector>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>

#include "convert/convert.h"
#include "log/Log.h"

class Version final {
public:
    Version(const int32_t major, const int32_t minor, const int32_t patch) : m_major(major), m_minor(minor), m_patch(patch) {}

    ~Version() = default;

    static Version parse(const std::string& version) {
        try {
            std::vector<std::string> split;
            boost::split(split, version, boost::is_any_of("."));

            const int32_t major = split.size() > 0 ? nexobar::convert<int32_t>(split[0]).value_or(0) : 0;
            const int32_t minor = split.size() > 1 ? nexobar::convert<int32_t>(split[1]).value_or(0) : 0;
            const int32_t patch = split.size() > 2 ? nexobar::convert<int32_t>(split[2]).value_or(0) : 0;

            return {major, minor, patch};
        } catch (...) {
            return {0, 0, 0};
        }
    }

    bool operator<(const Version& other) const {
        if (m_major != other.m_major) return m_major < other.m_major;
        if (m_minor != other.m_minor) return m_minor < other.m_minor;
        return m_patch < other.m_patch;
    }

    bool operator>(const Version& other) const {
        if (m_major != other.m_major) return m_major > other.m_major;
        if (m_minor != other.m_minor) return m_minor > other.m_minor;
        return m_patch > other.m_patch;
    }

    bool operator==(const Version& other) const {
        return m_major == other.m_major && m_minor == other.m_minor && m_patch == other.m_patch;
    }

    [[nodiscard]] bool need2Update(const Version& other) const {
        return *this > other;
    }

    [[nodiscard]] std::string version() const {
        std::ostringstream oss;
        oss << m_major << "." << m_minor << "." << m_patch;
        return oss.str();
    }


    [[nodiscard]] int32_t major() const { return m_major; }
    [[nodiscard]] int32_t minor() const { return m_minor; }
    [[nodiscard]] int32_t patch() const { return m_patch; }

    void print() const {
        NEXO_INFO(LogModule::Core) << "";
        NEXO_INFO(LogModule::Core) << "==================================";
        NEXO_INFO(LogModule::Core) << "NexoBar Version: " << version();
        NEXO_INFO(LogModule::Core) << "==================================";
        NEXO_INFO(LogModule::Core) << "";
    }

private:
    int32_t m_major;
    int32_t m_minor;
    int32_t m_patch;
};
