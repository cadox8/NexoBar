#include "FontLoader.h"

#include <array>
#include <stdexcept>

FontLoader::FontLoader(std::filesystem::path root) : m_root(std::move(root)) {}
void FontLoader::root(std::filesystem::path value) { m_root = std::move(value); }
void FontLoader::registerResource(std::string key, std::filesystem::path path, const int size) {
    if (key.empty() || size <= 0) throw std::invalid_argument("Invalid font resource");
    m_paths.insert_or_assign(std::move(key), Definition{std::move(path), size});
}
bool FontLoader::contains(const std::string& key) const { return m_paths.contains(key); }
FontPtr FontLoader::fallback() {
    if (const auto it = m_cache.find("default"); it != m_cache.end()) return it->second;
    auto result = FontPtr(new Font(GetFontDefault()), [](Font* font) { delete font; });
    m_cache.emplace("default", result);
    return result;
}
FontPtr FontLoader::get(const std::string& key) {
    if (const auto loaded = m_cache.find(key); loaded != m_cache.end()) return loaded->second;
    const auto registered = m_paths.find(key);
    if (registered == m_paths.end()) {
        if (key == "default") return fallback();
        throw std::out_of_range("Unknown font key: " + key);
    }
    const auto path = registered->second.path.is_absolute()
        ? registered->second.path : m_root / "fonts" / registered->second.path;
    // Load printable ASCII and Latin-1, excluding the non-rendering soft hyphen,
    // plus the extra symbols used by the UI. Do not request C0/C1 control codes:
    // fonts intentionally have no glyphs for them and raylib reports a warning.
    std::array<int, 192> codepoints{};
    std::size_t index = 0;
    for (int codepoint = 32; codepoint <= 126; ++codepoint) codepoints[index++] = codepoint;
    for (int codepoint = 160; codepoint <= 255; ++codepoint)
        if (codepoint != 0x00AD) codepoints[index++] = codepoint;
    codepoints[index++] = 0x20AC; // Euro sign
    codepoints[index] = 0x2026; // Ellipsis
    const Font value = LoadFontEx(path.string().c_str(), registered->second.size,
                                  codepoints.data(), static_cast<int>(codepoints.size()));
    if (!value.texture.id) throw std::runtime_error("Could not load font: " + path.string());
    SetTextureFilter(value.texture, TEXTURE_FILTER_BILINEAR);
    auto result = FontPtr(new Font(value), [](Font* font) {
        if (font->texture.id && IsWindowReady()) UnloadFont(*font);
        delete font;
    });
    m_cache.emplace(key, result);
    return result;
}
void FontLoader::clear() { m_cache.clear(); m_paths.clear(); }

