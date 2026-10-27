#include "MusicLoader.h"

#include <stdexcept>

MusicLoader::MusicLoader(std::filesystem::path root) : m_root(std::move(root)) {}
void MusicLoader::root(std::filesystem::path value) { m_root = std::move(value); }
void MusicLoader::registerResource(std::string key, std::filesystem::path path) {
    if (key.empty()) throw std::invalid_argument("Music key cannot be empty");
    m_paths.insert_or_assign(std::move(key), std::move(path));
}
bool MusicLoader::contains(const std::string& key) const { return m_paths.contains(key); }
MusicPtr MusicLoader::get(const std::string& key) {
    if (const auto loaded = m_cache.find(key); loaded != m_cache.end()) return loaded->second;
    const auto registered = m_paths.find(key);
    if (registered == m_paths.end()) throw std::out_of_range("Unknown music key: " + key);
    const auto path = registered->second.is_absolute() ? registered->second : m_root / "music" / registered->second;
    const Music value = LoadMusicStream(path.string().c_str());
    if (!value.frameCount) throw std::runtime_error("Could not load music: " + path.string());
    auto result = MusicPtr(new Music(value), [](Music* music) {
        if (music->frameCount && IsAudioDeviceReady()) UnloadMusicStream(*music);
        delete music;
    });
    m_cache.emplace(key, result);
    return result;
}
void MusicLoader::clear() { m_cache.clear(); m_paths.clear(); }
