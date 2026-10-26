#pragma once
#include <raylib.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>
#include <algorithm>
#include <cstddef>
#include <filesystem>

using TexturePtr = std::shared_ptr<Texture2D>;
using FontPtr = std::shared_ptr<Font>;
using SoundPtr = std::shared_ptr<Sound>;
using MusicPtr = std::shared_ptr<Music>;

class AssetManager {
public:
    explicit AssetManager(std::filesystem::path resourceRoot = {});

    AssetManager(const AssetManager&) = delete;

    AssetManager& operator=(const AssetManager&) = delete;

    void registerTexture(std::string key, std::string path);

    void registerFont(std::string key, std::string path, int size = 32);

    void registerSound(std::string key, std::string path);

    void registerMusic(std::string key, std::string path);

    void discoverResources(); // Registers files found under resources/assets, fonts, sounds and music.
    bool preloadNext(); // Loads the built-in font or one registered asset on the calling thread.
    void preloadAll();

    TexturePtr getTexture(const std::string& key);

    FontPtr getFont(const std::string& key);

    SoundPtr getSound(const std::string& key);

    MusicPtr getMusic(const std::string& key);

    bool ready() const noexcept { return m_defaultFontReady && m_nextJob >= m_jobs.size(); }
    std::size_t pendingCount() const noexcept { return (m_defaultFontReady ? 0u : 1u) + (m_jobs.size() - std::min(m_nextJob, m_jobs.size())); }

    std::string loadingStatus() const;

    std::size_t completedCount() const noexcept { return (m_defaultFontReady ? 1u : 0u) + m_nextJob; }
    std::size_t totalCount() const noexcept { return m_jobs.size() + 1u; }

    void clear();

private:
    void addKey(const std::string& key);

    std::string resolvePath(const std::string& folder, const std::string& value) const;

    TexturePtr loadTexture(const std::string& path) const;

    FontPtr loadFont(const std::string& path, int size) const;

    SoundPtr loadSound(const std::string& path) const;

    MusicPtr loadMusic(const std::string& path) const;

    bool m_defaultFontReady = false;
    std::size_t m_nextJob = 0;
    std::unordered_map<std::string, std::string> m_texturePaths, m_soundPaths, m_musicPaths;
    std::unordered_map<std::string, std::pair<std::string, int> > m_fontPaths;
    std::unordered_map<std::string, TexturePtr> m_textures;
    std::unordered_map<std::string, FontPtr> m_fonts;
    std::unordered_map<std::string, SoundPtr> m_sounds;
    std::unordered_map<std::string, MusicPtr> m_music;
    std::vector<std::function<void()> > m_jobs;
    std::vector<std::string> m_jobDescriptions;
    std::string m_loadingStatus = "Cargando fuentes y recursos...";
    std::filesystem::path m_resourceRoot;
};

using AssetManagerPtr = std::shared_ptr<AssetManager>;
