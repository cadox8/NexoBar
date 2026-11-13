#include "AssetManager.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace {
    std::filesystem::path applicationDirectory() {
        const char* directory = GetApplicationDirectory();
        if (directory != nullptr && directory[0] != '\0') return std::filesystem::path(directory);
        return std::filesystem::current_path();
    }
}

AssetManager::AssetManager(std::filesystem::path root) {
    if (root.empty()) {
        const auto cwd = std::filesystem::current_path();
        const auto executableDirectory = applicationDirectory();
        const std::filesystem::path candidates[] = {
            executableDirectory / "resources", cwd / "resources", cwd / "NexoBar" / "resources",
            cwd / "bin" / "resources", cwd / ".." / "bin" / "resources"
        };
        for (const auto& candidate: candidates) {
            if (std::filesystem::is_directory(candidate)) {
                root = candidate;
                break;
            }
        }
        if (root.empty()) root = cwd / "resources";
    }
    m_resourceRoot = std::filesystem::absolute(std::move(root));
    if (!std::filesystem::is_directory(m_resourceRoot))
        throw std::runtime_error("Resource directory not found: " + m_resourceRoot.string());
    m_textures.root(m_resourceRoot);
    m_fonts.root(m_resourceRoot);
    m_sounds.root(m_resourceRoot);
    m_music.root(m_resourceRoot);
}

void AssetManager::addJob(const Type type, const std::string& key, std::function<void()> load) {
    m_jobs.push_back(Job{.type = type, .key = key, .load = std::move(load)});
}

void AssetManager::registerTexture(std::string key, std::string path) {
    if (m_textures.contains(key)) throw std::invalid_argument("Duplicate texture key: " + key);
    m_textures.registerResource(key, std::move(path));
    addJob(Type::Texture, key, [this, key] { m_textures.get(key); });
}

void AssetManager::registerFont(std::string key, std::string path, const int size) {
    if (m_fonts.contains(key)) throw std::invalid_argument("Duplicate font key: " + key);
    m_fonts.registerResource(key, std::move(path), size);
    addJob(Type::Font, key, [this, key] { m_fonts.get(key); });
}

void AssetManager::registerSound(std::string key, std::string path) {
    if (m_sounds.contains(key)) throw std::invalid_argument("Duplicate sound key: " + key);
    m_sounds.registerResource(key, std::move(path));
    addJob(Type::Sound, key, [this, key] { m_sounds.get(key); });
}

void AssetManager::registerMusic(std::string key, std::string path) {
    if (m_music.contains(key)) throw std::invalid_argument("Duplicate music key: " + key);
    m_music.registerResource(key, std::move(path));
    addJob(Type::Music, key, [this, key] { m_music.get(key); });
}

void AssetManager::discoverResources() {
    struct Definition {
        const char* folder;
        std::vector<std::string> extensions;
        Type type;
    };
    const Definition definitions[] = {
        {"assets", {".png", ".jpg", ".jpeg", ".bmp", ".tga", ".webp"}, Type::Texture},
        {"fonts", {".ttf", ".otf"}, Type::Font},
        {"sounds", {".wav", ".mp3", ".ogg", ".flac"}, Type::Sound},
        {"music", {".wav", ".mp3", ".ogg", ".flac", ".xm", ".mod"}, Type::Music}
    };
    for (const auto& definition: definitions) {
        const auto directory = m_resourceRoot / definition.folder;
        if (!std::filesystem::is_directory(directory)) continue;
        for (const auto& entry: std::filesystem::recursive_directory_iterator(directory)) {
            if (!entry.is_regular_file()) continue;
            std::string extension = entry.path().extension().string();
            std::ranges::transform(extension, extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (std::ranges::find(definition.extensions, extension) == definition.extensions.end()) continue;
            auto relative = std::filesystem::relative(entry.path(), directory);
            relative.replace_extension();
            std::string key = relative.generic_string();
            const auto available = [&] {
                switch (definition.type) {
                    case Type::Texture: return !m_textures.contains(key);
                    case Type::Font: return !m_fonts.contains(key);
                    case Type::Sound: return !m_sounds.contains(key);
                    case Type::Music: return !m_music.contains(key);
                }
                return false;
            }();
            if (!available) key += extension;
            // Discovered assets are registered for on-demand use. Only resources
            // explicitly registered by the application join the startup preload,
            // avoiding dozens of unused fonts/audio files resident in memory.
            switch (definition.type) {
                case Type::Texture: if (!m_textures.contains(key)) m_textures.registerResource(key, entry.path());
                    break;
                case Type::Font: if (!m_fonts.contains(key)) m_fonts.registerResource(key, entry.path(), 32);
                    break;
                case Type::Sound: if (!m_sounds.contains(key)) m_sounds.registerResource(key, entry.path());
                    break;
                case Type::Music: if (!m_music.contains(key)) m_music.registerResource(key, entry.path());
                    break;
            }
        }
    }
}

bool AssetManager::preloadNext() {
    if (m_nextJob >= m_jobs.size()) {
        m_loadingResource.clear();
        return true;
    }
    m_loadingResource = m_jobs[m_nextJob].key;
    m_jobs[m_nextJob++].load();
    return m_nextJob >= m_jobs.size();
}

void AssetManager::preloadAll() { while (!preloadNext()) {} }
bool AssetManager::ready() const noexcept { return m_nextJob >= m_jobs.size(); }
std::size_t AssetManager::pendingCount() const noexcept { return m_jobs.size() - std::min(m_nextJob, m_jobs.size()); }
std::size_t AssetManager::completedCount() const noexcept { return m_nextJob; }
std::size_t AssetManager::totalCount() const noexcept { return m_jobs.size(); }

void AssetManager::clear() {
    m_music.clear();
    m_sounds.clear();
    m_fonts.clear();
    m_textures.clear();
    m_jobs.clear();
    m_nextJob = 0;
    m_loadingResource.clear();
}
