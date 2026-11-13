#pragma once

#include "ResourceTypes.h"

#include <filesystem>
#include <string>
#include <unordered_map>

class MusicLoader {
public:
    explicit MusicLoader(std::filesystem::path root = {});
    void root(std::filesystem::path value);
    void registerResource(std::string key, std::filesystem::path path);
    [[nodiscard]] bool contains(const std::string& key) const;
    MusicPtr get(const std::string& key);
    void clear();
private:
    std::filesystem::path m_root;
    std::unordered_map<std::string, std::filesystem::path> m_paths;
    std::unordered_map<std::string, MusicPtr> m_cache;
};

