#pragma once

#include "ResourceTypes.h"

#include <filesystem>
#include <string>
#include <unordered_map>

class FontLoader {
public:
    explicit FontLoader(std::filesystem::path root = {});
    void root(std::filesystem::path value);
    void registerResource(std::string key, std::filesystem::path path, int size = 32);
    [[nodiscard]] bool contains(const std::string& key) const;
    FontPtr get(const std::string& key);
    FontPtr fallback();
    void clear();

private:
    struct Definition { std::filesystem::path path; int size = 32; };
    std::filesystem::path m_root;
    std::unordered_map<std::string, Definition> m_paths;
    std::unordered_map<std::string, FontPtr> m_cache;
};

