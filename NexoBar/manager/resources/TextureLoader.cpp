#include "TextureLoader.h"

#include <stdexcept>

TextureLoader::TextureLoader(std::filesystem::path root) : m_root(std::move(root)) {}
void TextureLoader::root(std::filesystem::path value) { m_root = std::move(value); }
void TextureLoader::registerResource(std::string key, std::filesystem::path path) {
    if (key.empty()) throw std::invalid_argument("Texture key cannot be empty");
    m_paths.insert_or_assign(std::move(key), std::move(path));
}
bool TextureLoader::contains(const std::string& key) const { return m_paths.contains(key); }
std::filesystem::path TextureLoader::resolve(const std::filesystem::path& path) const {
    return path.is_absolute() ? path : m_root / "assets" / path;
}
TexturePtr TextureLoader::get(const std::string& key) {
    if (const auto loaded = m_cache.find(key); loaded != m_cache.end()) return loaded->second;
    const auto registered = m_paths.find(key);
    if (registered == m_paths.end()) throw std::out_of_range("Unknown texture key: " + key);
    const auto path = resolve(registered->second);
    Texture2D value = LoadTexture(path.string().c_str());
    // SVG support depends on how raylib was compiled. Keep SVG as the preferred
    // source, but transparently use the bundled raster copy on builds where the
    // SVG decoder is disabled (notably some vcpkg binaries).
    if (!value.id && path.extension() == ".svg") {
        auto fallback = path;
        fallback.replace_extension(".png");
        if (std::filesystem::is_regular_file(fallback))
            value = LoadTexture(fallback.string().c_str());
    }
    if (!value.id) throw std::runtime_error("Could not load texture (SVG/PNG): " + path.string());
    auto result = TexturePtr(new Texture2D(value), [](Texture2D* texture) {
        if (texture->id && IsWindowReady()) UnloadTexture(*texture);
        delete texture;
    });
    m_cache.emplace(key, result);
    return result;
}
void TextureLoader::clear() { m_cache.clear(); m_paths.clear(); }

