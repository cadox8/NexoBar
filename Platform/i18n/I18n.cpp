#include "I18n.h"

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#endif

namespace platform {
    namespace {
    class Catalog {
    public:
        nlohmann::json messages;

        explicit Catalog(const std::filesystem::path& path) {
            std::ifstream file(path, std::ios::binary);
            if (!file) throw std::runtime_error("Cannot open language catalog: " + path.string());
            nlohmann::json root;
            file >> root;
            if (!root.is_object() || !root.contains("messages") || !root["messages"].is_object())
                throw std::runtime_error("Language catalog must contain a messages object: " + path.string());
            messages = std::move(root["messages"]);
        }

        [[nodiscard]] std::string get(std::string_view key) const {
            const nlohmann::json* value = &messages;
            std::size_t start = 0;
            while (start <= key.size()) {
                const std::size_t end = key.find('.', start);
                const std::string part(key.substr(start, end == std::string_view::npos ? key.size() - start : end - start));
                if (!value->is_object() || !value->contains(part)) return {};
                value = &value->at(part);
                if (end == std::string_view::npos) break;
                start = end + 1;
            }
            return value->is_string() ? value->get<std::string>() : std::string{};
        }
    };

        std::string normalizeLocale(std::string locale) {
            for (char& c : locale) {
                if (c == '_') c = '-';
            }
            if (locale.empty()) return "en-US";
            return locale;
        }

        std::filesystem::path findCatalog(const std::filesystem::path& directory, const std::string& locale) {
            const std::string normalized = normalizeLocale(locale);
            std::vector candidates{normalized};
            if (normalized == "es") candidates.emplace_back("es-ES");
            if (normalized == "en") candidates.emplace_back("en-US");
            if (const std::size_t dash = normalized.find('-'); dash != std::string::npos) {
                const std::string language = normalized.substr(0, dash);
                for (const auto& entry : std::filesystem::directory_iterator(directory)) {
                    if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;
                    const std::string stem = entry.path().stem().string();
                    if (stem == language || stem.starts_with(language + "-")) candidates.emplace_back(stem);
                }
            }
            for (const std::string& candidate : candidates) {
                const auto path = directory / (candidate + ".json");
                if (std::filesystem::is_regular_file(path)) return path;
            }
            return {};
        }

        std::shared_ptr<const Catalog> loadCatalog(const std::filesystem::path& directory,
                                                   const std::string& locale,
                                                   std::filesystem::path& path) {
            path = findCatalog(directory, locale);
            if (path.empty()) return {};
            return std::make_shared<Catalog>(path);
        }
    }

    class I18n::Impl {
    public:
        std::filesystem::path catalogPath;
        std::string locale;
        std::shared_ptr<const Catalog> catalog;
        std::shared_ptr<const Catalog> fallback;

        Impl(const std::filesystem::path& directory, std::string requestedLocale, std::string fallbackLocale) {
            fallbackLocale = normalizeLocale(std::move(fallbackLocale));
            requestedLocale = normalizeLocale(std::move(requestedLocale));
            catalog = loadCatalog(directory, requestedLocale, catalogPath);
            if (!catalog) {
                catalog = loadCatalog(directory, fallbackLocale, catalogPath);
                if (!catalog)
                    throw std::runtime_error("No catalog for locale '" + requestedLocale + "' and no fallback '" + fallbackLocale + "'");
            }
            locale = catalogPath.stem().string();

            std::filesystem::path ignored;
            fallback = loadCatalog(directory, fallbackLocale, ignored);
            if (!fallback) fallback = catalog;
        }
    };

    I18n::I18n(const std::filesystem::path& catalogDirectory, std::string requestedLocale, std::string fallbackLocale)
        : m_impl(std::make_unique<Impl>(catalogDirectory, std::move(requestedLocale), std::move(fallbackLocale))) {}

    I18n::~I18n() = default;
    I18n::I18n(I18n&&) noexcept = default;
    I18n& I18n::operator=(I18n&&) noexcept = default;

    std::string I18n::get(const std::string_view key) const {
        if (std::string value = m_impl->catalog->get(key); !value.empty()) return value;
        if (m_impl->fallback != m_impl->catalog) {
            if (std::string value = m_impl->fallback->get(key); !value.empty()) return value;
        }
        return std::string(key);
    }

    const std::string& I18n::locale() const noexcept { return m_impl->locale; }
    const std::filesystem::path& I18n::catalogPath() const noexcept { return m_impl->catalogPath; }

    std::string I18n::systemLanguage() {
#ifdef _WIN32
        ULONG languageCount = 0;
        ULONG characterCount = 0;
        if (GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &languageCount, nullptr, &characterCount)
            && characterCount > 1) {
            std::vector<wchar_t> preferredLanguages(characterCount);
            if (GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &languageCount, preferredLanguages.data(), &characterCount)) {
                const std::wstring preferred(preferredLanguages.data());
                const int bytes = WideCharToMultiByte(CP_UTF8, 0, preferred.c_str(), -1, nullptr, 0, nullptr, nullptr);
                if (bytes > 1) {
                    std::string locale(static_cast<std::size_t>(bytes), '\0');
                    WideCharToMultiByte(CP_UTF8, 0, preferred.c_str(), -1, locale.data(), bytes, nullptr, nullptr);
                    locale.resize(static_cast<std::size_t>(bytes - 1));
                    return normalizeLocale(std::move(locale));
                }
            }
        }

        // Fall back to the Windows regional locale if preferred UI languages are unavailable.
        wchar_t localeName[LOCALE_NAME_MAX_LENGTH]{};
        if (GetUserDefaultLocaleName(localeName, LOCALE_NAME_MAX_LENGTH) > 0) {
            const int bytes = WideCharToMultiByte(CP_UTF8, 0, localeName, -1, nullptr, 0, nullptr, nullptr);
            if (bytes > 1) {
                std::string locale(static_cast<std::size_t>(bytes), '\0');
                WideCharToMultiByte(CP_UTF8, 0, localeName, -1, locale.data(), bytes, nullptr, nullptr);
                locale.resize(static_cast<std::size_t>(bytes - 1));
                return normalizeLocale(std::move(locale));
            }
        }
#else
        if (const char* locale = std::getenv("LC_ALL"); locale && *locale) return normalizeLocale(locale);
        if (const char* locale = std::getenv("LC_MESSAGES"); locale && *locale) return normalizeLocale(locale);
        if (const char* locale = std::getenv("LANG"); locale && *locale) {
            std::string result(locale);
            if (const std::size_t dot = result.find('.'); dot != std::string::npos) result.resize(dot);
            return normalizeLocale(std::move(result));
        }
#endif
        return "en-US";
    }
}
