#pragma once

#include "ResourceTypes.h"

#include <filesystem>
#include <string>
#include <unordered_map>

class TextureLoader {
public:
    explicit TextureLoader(std::filesystem::path root = {});
    void root(std::filesystem::path value);
    void registerResource(std::string key, std::filesystem::path path);
    [[nodiscard]] bool contains(const std::string& key) const;
    TexturePtr get(const std::string& key);
    void clear();

private:
    [[nodiscard]] std::filesystem::path resolve(const std::filesystem::path& path) const;
    std::filesystem::path m_root;
    std::unordered_map<std::string, std::filesystem::path> m_paths;
    std::unordered_map<std::string, TexturePtr> m_cache;
};

