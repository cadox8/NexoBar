#pragma once

#include "../export.h"

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

namespace platform {
    /// Loads a JSON translation catalog, choosing the requested/system locale and falling back to English.
    class I18n {
    public:
        PLATFORM_API explicit I18n(const std::filesystem::path& catalogDirectory,
                                   std::string requestedLocale = systemLanguage(),
                                   std::string fallbackLocale = "en-US");
        PLATFORM_API ~I18n();

        PLATFORM_API I18n(I18n&&) noexcept;
        PLATFORM_API I18n& operator=(I18n&&) noexcept;
        I18n(const I18n&) = delete;
        I18n& operator=(const I18n&) = delete;

        [[nodiscard]] PLATFORM_API std::string get(std::string_view key) const;
        [[nodiscard]] PLATFORM_API const std::string& locale() const noexcept;
        [[nodiscard]] PLATFORM_API const std::filesystem::path& catalogPath() const noexcept;

        [[nodiscard]] PLATFORM_API static std::string systemLanguage();

    private:
        class Impl;
        std::unique_ptr<Impl> m_impl;
    };
}
