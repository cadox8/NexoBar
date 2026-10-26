#include "SplashScreen.h"

#include "../../../NexoBar.h"
#include <raylib.h>
#include <algorithm>
#include <stdexcept>
#include <string>

void SplashScreen::update(const float deltaSeconds) {
    NexoBar* app = NexoBarImp::get();
    if (!app)
        return;

    if (!m_assetsLoaded) {
        m_assetsLoaded = app->assets().preloadNext();
        if (m_assetsLoaded) {
            const TexturePtr source = app->assets().getTexture("nexobar-icon");
            Image image = LoadImageFromTexture(*source);
            if (!image.data)
                throw std::runtime_error("Could not prepare the splash logo image");
            ImageAlphaCrop(&image, 0.1f);
            const Texture2D texture = LoadTextureFromImage(image);
            UnloadImage(image);
            if (!texture.id)
                throw std::runtime_error("Could not prepare the splash logo texture");
            m_logo = TexturePtr(new Texture2D(texture), [](Texture2D* loaded) {
                UnloadTexture(*loaded);
                delete loaded;
            });
        }
    }

    if (m_assetsLoaded)
        m_elapsed += deltaSeconds;

    if (!m_transitioned && m_assetsLoaded && m_elapsed >= 3.f) {
        m_transitioned = true;
        app->screens().changeScreen(app->startupErrors().empty() ? "catalog" : "errors");
    }
}

void SplashScreen::draw() {
    NexoBar* app = NexoBarImp::get();
    const FontPtr font = app ? app->defaultFont() : FontPtr{};
    const int width = GetScreenWidth();
    const int height = GetScreenHeight();

    if (m_logo&& m_logo->id) {
        const float scale = std::min(static_cast<float>(width) * .72f / static_cast<float>(m_logo->width),
                                     static_cast<float>(height) * .55f / static_cast<float>(m_logo->height));
        const float logoWidth = static_cast<float>(m_logo->width) * scale;
        const float logoHeight = static_cast<float>(m_logo->height) * scale;
        const Rectangle source{0, 0, static_cast<float>(m_logo->width), static_cast<float>(m_logo->height)};
        const Rectangle destination{
            (static_cast<float>(width) - logoWidth) * .5f,
            (static_cast<float>(height) - logoHeight) * .5f,
            logoWidth,
            logoHeight
        };
        DrawTexturePro(*m_logo, source, destination, {0, 0}, 0.f, WHITE);
    } else {
        const char* title = "NexoBar";
        if (font) {
            const Vector2 size = MeasureTextEx(*font, title, 56, 1);
            DrawTextEx(*font, title, {(width - size.x) * .5f, (height - size.y) * .5f}, 56, 1, RAYWHITE);
        } else {
            DrawText(title, (width - MeasureText(title, 48)) / 2, height / 2 - 24, 48, RAYWHITE);
        }
    }

    const std::string status = app
                                   ? (m_assetsLoaded
                                          ? app->text("splash.preparing", "Preparando el punto de venta...")
                                          : app->assets().loadingStatus())
                                   : "Cargando recursos...";
    const Color statusColor{.r = 190, .g = 199, .b = 214, .a = 255};
    if (font) {
        const Vector2 size = MeasureTextEx(*font, status.c_str(), 22, 1);
        DrawTextEx(*font, status.c_str(), {.x = (width - size.x) * .5f, .y = height - 38.f}, 22, 1, statusColor);
    } else {
        DrawText(status.c_str(), (width - MeasureText(status.c_str(), 18)) / 2, height - 34, 18, statusColor);
    }
}
