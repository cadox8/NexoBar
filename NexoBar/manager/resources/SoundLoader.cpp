#include "SoundLoader.h"

#include <stdexcept>

SoundLoader::SoundLoader(std::filesystem::path root) : m_root(std::move(root)) {}
void SoundLoader::root(std::filesystem::path value) { m_root = std::move(value); }
void SoundLoader::registerResource(std::string key, std::filesystem::path path) {
    if (key.empty()) throw std::invalid_argument("Sound key cannot be empty");
    m_paths.insert_or_assign(std::move(key), std::move(path));
}
bool SoundLoader::contains(const std::string& key) const { return m_paths.contains(key); }
SoundPtr SoundLoader::get(const std::string& key) {
    if (const auto loaded = m_cache.find(key); loaded != m_cache.end()) return loaded->second;
    const auto registered = m_paths.find(key);
    if (registered == m_paths.end()) throw std::out_of_range("Unknown sound key: " + key);
    const auto path = registered->second.is_absolute() ? registered->second : m_root / "sounds" / registered->second;
    const Sound value = LoadSound(path.string().c_str());
    if (!value.frameCount) throw std::runtime_error("Could not load sound: " + path.string());
    auto result = SoundPtr(new Sound(value), [](Sound* sound) {
        if (sound->frameCount && IsAudioDeviceReady()) UnloadSound(*sound);
        delete sound;
    });
    m_cache.emplace(key, result);
    return result;
}
void SoundLoader::clear() { m_cache.clear(); m_paths.clear(); }

