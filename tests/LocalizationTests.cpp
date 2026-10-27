#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <array>
#include <regex>
#include <set>
#include <sstream>
#include <string>

namespace {
using KeySet = std::set<std::string>;

nlohmann::json readJson(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    EXPECT_TRUE(input.good()) << path.string();
    nlohmann::json value;
    input >> value;
    return value;
}

void collectKeys(const nlohmann::json& value, const std::string& prefix, KeySet& keys) {
    for (const auto& [name, child] : value.items()) {
        const std::string key = prefix.empty() ? name : prefix + '.' + name;
        if (child.is_object())
            collectKeys(child, key, keys);
        else if (child.is_string())
            keys.insert(key);
    }
}
}

TEST(LocalizationTests, CatalogsMatchAndCoverLiteralTranslationKeys) {
    const std::filesystem::path languageDirectory = NEXOBAR_TEST_LANGUAGE_DIR;
    const auto spanish = readJson(languageDirectory / "es-ES.json");
    const auto english = readJson(languageDirectory / "en-US.json");
    ASSERT_TRUE(spanish.contains("messages"));
    ASSERT_TRUE(english.contains("messages"));

    KeySet spanishKeys, englishKeys;
    collectKeys(spanish.at("messages"), {}, spanishKeys);
    collectKeys(english.at("messages"), {}, englishKeys);
    EXPECT_EQ(spanishKeys, englishKeys);

    const std::regex translationCall(
        R"KEY((?:text|i18n\.get|->get)\s*\(\s*"([a-z][a-z0-9_]*(?:\.[a-z0-9_]+)+)")KEY");
    const std::filesystem::path sourceRoot = NEXOBAR_TEST_SOURCE_DIR;
    for (const std::string directory : {"NexoBar", "Management", "Platform"}) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(sourceRoot / directory)) {
            if (!entry.is_regular_file()) continue;
            const auto extension = entry.path().extension();
            if (extension != ".cpp" && extension != ".h") continue;
            std::ifstream input(entry.path(), std::ios::binary);
            std::ostringstream content;
            content << input.rdbuf();
            const std::string source = content.str();
            for (std::sregex_iterator match(source.begin(), source.end(), translationCall), end;
                 match != end; ++match) {
                const std::string key = (*match)[1].str();
                EXPECT_TRUE(spanishKeys.contains(key))
                    << entry.path().string() << " references missing translation key " << key;
            }
        }
    }
}

TEST(LocalizationTests, PresentationComponentsDoNotEmbedVisibleWords) {
    const std::filesystem::path sourceRoot = NEXOBAR_TEST_SOURCE_DIR;
    const std::array forbiddenPatterns{
        std::regex(R"KEY(std::make_shared<(?:Button|StaticText)>\s*\(\s*"([^"]*[A-Za-z][^"]*)")KEY"),
        std::regex(R"KEY((?:->text|->placeholder|\.placeholder)\s*=\s*"([^"]*[A-Za-z][^"]*)")KEY"),
        std::regex(R"KEY(ftxui::(?:text|paragraph)\s*\(\s*"([^"]*[A-Za-z][^"]*)")KEY")
    };
    for (const std::string directory : {"NexoBar/ui"}) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(sourceRoot / directory)) {
            if (!entry.is_regular_file()) continue;
            const auto extension = entry.path().extension();
            if (extension != ".cpp" && extension != ".h") continue;
            std::ifstream input(entry.path(), std::ios::binary);
            std::ostringstream content;
            content << input.rdbuf();
            const std::string source = content.str();
            for (const auto& pattern : forbiddenPatterns) {
                std::smatch match;
                EXPECT_FALSE(std::regex_search(source, match, pattern))
                    << entry.path().string() << " embeds visible text: "
                    << (match.empty() ? std::string{} : match[1].str());
            }
        }
    }
}
