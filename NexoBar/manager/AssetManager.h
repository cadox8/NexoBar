#pragma once

#include "resources/FontLoader.h"
#include "resources/MusicLoader.h"
#include "resources/SoundLoader.h"
#include "resources/TextureLoader.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

class AssetManager {
public:
    explicit AssetManager(std::filesystem::path resourceRoot = {});
    AssetManager(const AssetManager&) = delete;
    AssetManager& operator=(const AssetManager&) = delete;

    void registerTexture(std::string key, std::string path);
    void registerFont(std::string key, std::string path, int size = 32);
    void registerSound(std::string key, std::string path);
    void registerMusic(std::string key, std::string path);
    void discoverResources();
    bool preloadNext();
    void preloadAll();

    [[nodiscard]] bool ready() const noexcept;
    [[nodiscard]] std::size_t pendingCount() const noexcept;
    [[nodiscard]] std::size_t completedCount() const noexcept;
    [[nodiscard]] std::size_t totalCount() const noexcept;
    [[nodiscard]] const std::string& loadingResource() const noexcept { return m_loadingResource; }

    TexturePtr getTexture(const std::string& key) { return m_textures.get(key); }
    FontPtr getFont(const std::string& key) { return m_fonts.get(key); }
    SoundPtr getSound(const std::string& key) { return m_sounds.get(key); }
    MusicPtr getMusic(const std::string& key) { return m_music.get(key); }
    TextureLoader& textures() noexcept { return m_textures; }
    FontLoader& fonts() noexcept { return m_fonts; }
    SoundLoader& sounds() noexcept { return m_sounds; }
    MusicLoader& music() noexcept { return m_music; }
    [[nodiscard]] const std::filesystem::path& resourceRoot() const noexcept { return m_resourceRoot; }
    void clear();

private:
    enum class Type { Texture, Font, Sound, Music };
    struct Job { Type type; std::string key; std::function<void()> load; };
    void addJob(Type type, const std::string& key, std::function<void()> load);

    std::filesystem::path m_resourceRoot;
    TextureLoader m_textures;
    FontLoader m_fonts;
    SoundLoader m_sounds;
    MusicLoader m_music;
    std::vector<Job> m_jobs;
    std::size_t m_nextJob = 0;
    std::string m_loadingResource;
};

using AssetManagerPtr = std::shared_ptr<AssetManager>;
