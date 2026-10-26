#include "AssetManager.h"
#include <filesystem>
#include <cctype>
#include <stdexcept>
#include <array>

AssetManager::AssetManager(std::filesystem::path root) {
    if (root.empty()) {
        const auto cwd = std::filesystem::current_path();
        const std::vector<std::filesystem::path> candidates = {cwd / "resources", cwd / "NexoBar" / "resources", cwd / "bin" / "resources", cwd / ".." / "bin" / "resources"};
        for (const auto& candidate: candidates)
            if (std::filesystem::is_directory(candidate)) {
                root = candidate;
                break;
            }
        if (root.empty())root = cwd / "resources";
    }
    m_resourceRoot = std::filesystem::absolute(std::move(root));
}

std::string AssetManager::resolvePath(const std::string& folder, const std::string& value) const {
    const std::filesystem::path p(value);
    if (p.is_absolute())return p.string();
    if (p.begin() != p.end() && *p.begin() == "resources")return (m_resourceRoot.parent_path() / p).string();
    if (p.begin() != p.end() && *p.begin() == folder)return (m_resourceRoot / p).string();
    return (m_resourceRoot / folder / p).string();
}

void AssetManager::addKey(const std::string& key) { if (key.empty())throw std::invalid_argument("Asset keys cannot be empty"); }

TexturePtr AssetManager::loadTexture(const std::string& path) const {
    auto* p = new Texture2D(LoadTexture(path.c_str()));
    if (!p->id) {
        delete p;
        throw std::runtime_error("Could not load texture: " + path);
    }
    return TexturePtr(p, [](Texture2D* v) {
        UnloadTexture(*v);
        delete v;
    });
}

FontPtr AssetManager::loadFont(const std::string& path, int size) const {
    std::array < int, 112 > codepoints{};
    int codepointCount = 0;
    for (int codepoint = 32; codepoint <= 126; ++codepoint)codepoints[codepointCount++] = codepoint;
    for (const int codepoint: {
             0x00a1, 0x00bf, 0x00c1, 0x00c9, 0x00cd, 0x00d1, 0x00d3, 0x00da, 0x00dc,
             0x00e1, 0x00e9, 0x00ed, 0x00f1, 0x00f3, 0x00fa, 0x00fc, 0x20ac
         })
        codepoints[codepointCount++] = codepoint;
    Font loaded = LoadFontEx(path.c_str(), size, codepoints.data(), codepointCount);
    if (loaded.texture.id)
        SetTextureFilter(loaded.texture, TEXTURE_FILTER_BILINEAR);
    auto* p = new Font(loaded);
    if (!p->texture.id) {
        delete p;
        throw std::runtime_error("Could not load font: " + path);
    }
    return FontPtr(p, [](Font* v) {
        UnloadFont(*v);
        delete v;
    });
}

SoundPtr AssetManager::loadSound(const std::string& path) const {
    auto* p = new Sound(LoadSound(path.c_str()));
    if (!p->stream.buffer) {
        delete p;
        throw std::runtime_error("Could not load sound: " + path);
    }
    return SoundPtr(p, [](Sound* v) {
        UnloadSound(*v);
        delete v;
    });
}

MusicPtr AssetManager::loadMusic(const std::string& path) const {
    auto* p = new Music(LoadMusicStream(path.c_str()));
    if (!p->stream.buffer) {
        delete p;
        throw std::runtime_error("Could not load music: " + path);
    }
    return MusicPtr(p, [](Music* v) {
        UnloadMusicStream(*v);
        delete v;
    });
}

void AssetManager::registerTexture(std::string key, std::string path) {
    addKey(key);
    if (m_texturePaths.contains(key))throw std::invalid_argument("Duplicate asset key: " + key);
    m_texturePaths[key] = resolvePath("assets", path);
    m_jobDescriptions.push_back("imagen " + key);
    m_jobs.push_back([this,key=std::move(key)] { if (!m_textures.contains(key))m_textures[key] = loadTexture(m_texturePaths.at(key)); });
}

void AssetManager::registerFont(std::string key, std::string path, int size) {
    addKey(key);
    if (m_fontPaths.contains(key))throw std::invalid_argument("Duplicate asset key: " + key);
    m_fontPaths[key] = {resolvePath("fonts", path), size};
    m_jobDescriptions.push_back("fuente " + key);
    m_jobs.push_back([this,key=std::move(key)] {
        const auto& p = m_fontPaths.at(key);
        if (!m_fonts.contains(key))m_fonts[key] = loadFont(p.first, p.second);
    });
}

void AssetManager::registerSound(std::string key, std::string path) {
    addKey(key);
    if (m_soundPaths.contains(key))throw std::invalid_argument("Duplicate asset key: " + key);
    m_soundPaths[key] = resolvePath("sounds", path);
    m_jobDescriptions.push_back("sonido " + key);
    m_jobs.push_back([this,key=std::move(key)] { if (!m_sounds.contains(key))m_sounds[key] = loadSound(m_soundPaths.at(key)); });
}

void AssetManager::registerMusic(std::string key, std::string path) {
    addKey(key);
    if (m_musicPaths.contains(key))throw std::invalid_argument("Duplicate asset key: " + key);
    m_musicPaths[key] = resolvePath("music", path);
    m_jobDescriptions.push_back("música " + key);
    m_jobs.push_back([this,key=std::move(key)] { if (!m_music.contains(key))m_music[key] = loadMusic(m_musicPaths.at(key)); });
}

void AssetManager::discoverResources() {
    const std::vector<std::pair<std::string, std::vector<std::string> > > types = {{"assets", {".png", ".jpg", ".jpeg", ".bmp", ".tga", ".webp"}}, {"fonts", {".ttf", ".otf"}}, {"sounds", {".wav", ".mp3", ".ogg", ".flac"}}, {"music", {".wav", ".mp3", ".ogg", ".flac", ".xm", ".mod"}}};
    for (const auto& [folder,extensions]: types) {
        const auto dir = m_resourceRoot / folder;
        if (!std::filesystem::is_directory(dir))continue;
        for (const auto& entry: std::filesystem::recursive_directory_iterator(dir)) {
            if (!entry.is_regular_file())continue;
            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return char(std::tolower(c)); });
            if (std::find(extensions.begin(), extensions.end(), ext) == extensions.end())continue;
            auto relative = std::filesystem::relative(entry.path(), dir);
            relative.replace_extension();
            std::string key = relative.generic_string();
            const std::string extKey = key + entry.path().extension().string();
            if (folder == "assets") {
                if (m_texturePaths.contains(key))key = extKey;
                if (!m_texturePaths.contains(key))registerTexture(key, entry.path().string());
            } else if (folder == "fonts") {
                if (m_fontPaths.contains(key))key = extKey;
                if (!m_fontPaths.contains(key))registerFont(key, entry.path().string());
            } else if (folder == "sounds") {
                if (m_soundPaths.contains(key))key = extKey;
                if (!m_soundPaths.contains(key))registerSound(key, entry.path().string());
            } else {
                if (m_musicPaths.contains(key))key = extKey;
                if (!m_musicPaths.contains(key))registerMusic(key, entry.path().string());
            }
        }
    }
}

bool AssetManager::preloadNext() {
    if (!m_defaultFontReady) {
        m_loadingStatus = "Cargando fuente predeterminada...";
        if (!m_fontPaths.contains("default"))m_fonts["default"] = FontPtr(new Font(GetFontDefault()), [](Font* f) { delete f; });
        m_defaultFontReady = true;
        return m_jobs.empty();
    }
    if (m_nextJob >= m_jobs.size())return true;
    m_loadingStatus = "Cargando " + m_jobDescriptions[m_nextJob] + " (" + std::to_string(m_nextJob + 1) + "/" + std::to_string(m_jobs.size()) + ")...";
    m_jobs[m_nextJob++]();
    if (m_nextJob >= m_jobs.size())m_loadingStatus = "Recursos listos. Preparando punto de venta...";
    return m_nextJob >= m_jobs.size();
}

std::string AssetManager::loadingStatus() const { return m_loadingStatus; }

void AssetManager::preloadAll() { while (!preloadNext()) {} }

TexturePtr AssetManager::getTexture(const std::string& key) {
    auto it = m_textures.find(key);
    if (it != m_textures.end())return it->second;
    auto path = m_texturePaths.find(key);
    if (path == m_texturePaths.end())throw std::out_of_range("Unknown texture key: " + key);
    return m_textures[key] = loadTexture(path->second);
}

FontPtr AssetManager::getFont(const std::string& key) {
    auto it = m_fonts.find(key);
    if (it != m_fonts.end())return it->second;
    auto p = m_fontPaths.find(key);
    if (p != m_fontPaths.end())return m_fonts[key] = loadFont(p->second.first, p->second.second);
    if (key == "default") {
        if (!m_defaultFontReady)preloadNext();
        return m_fonts.at("default");
    }
    throw std::out_of_range("Unknown font key: " + key);
}

SoundPtr AssetManager::getSound(const std::string& key) {
    auto it = m_sounds.find(key);
    if (it != m_sounds.end())return it->second;
    auto p = m_soundPaths.find(key);
    if (p == m_soundPaths.end())throw std::out_of_range("Unknown sound key: " + key);
    return m_sounds[key] = loadSound(p->second);
}

MusicPtr AssetManager::getMusic(const std::string& key) {
    auto it = m_music.find(key);
    if (it != m_music.end())return it->second;
    auto p = m_musicPaths.find(key);
    if (p == m_musicPaths.end())throw std::out_of_range("Unknown music key: " + key);
    return m_music[key] = loadMusic(p->second);
}

void AssetManager::clear() {
    m_music.clear();
    m_sounds.clear();
    m_fonts.clear();
    m_textures.clear();
    m_musicPaths.clear();
    m_soundPaths.clear();
    m_fontPaths.clear();
    m_texturePaths.clear();
    m_jobs.clear();
    m_jobDescriptions.clear();
    m_loadingStatus = "Cargando fuentes y recursos...";
    m_nextJob = 0;
    m_defaultFontReady = false;
}
