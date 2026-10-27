#include "SplashScreen.h"

#include "../../../NexoBar.h"
#include <raylib.h>
#include <algorithm>
#include <stdexcept>
#include <string>

void SplashScreen::update(const float deltaSeconds) {
    NexoBarImp nexo;
    if (!nexo)
        return;

    m_elapsed += deltaSeconds;

    if (!m_assetsLoaded) {
        m_assetsLoaded = nexo->assets().preloadNext();
        if (m_assetsLoaded) {
            const TexturePtr source = nexo->assets().getTexture("nexobar-icon");
            Image image = LoadImageFromTexture(*source);
            if (!image.data)
                throw std::runtime_error(nexo->text("errors.splash_image"));
            ImageAlphaCrop(&image, 0.1f);
            const Texture2D texture = LoadTextureFromImage(image);
            UnloadImage(image);
            if (!texture.id)
                throw std::runtime_error(nexo->text("errors.splash_texture"));
            m_logo = TexturePtr(new Texture2D(texture), [](Texture2D* loaded) {
                UnloadTexture(*loaded);
                delete loaded;
            });
        }
    }

    if (!m_transitioned && m_elapsed >= 3.f) {
        m_transitioned = true;
        nexo->screens().changeScreen(m_assetsLoaded ? (nexo->canEnterSession() ? "login" : "errors") : "loading");
    }
}

void SplashScreen::draw() {
    NexoBarImp nexo;
    const FontPtr font = nexo ? nexo->defaultFont() : FontPtr{};
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
        const std::string titleText = nexo ? nexo->text("app.name") : std::string{};
        const char* title = titleText.c_str();
        if (font) {
            const Vector2 size = MeasureTextEx(*font, title, 56, 1);
            DrawTextEx(*font, title, {(width - size.x) * .5f, (height - size.y) * .5f}, 56, 1, RAYWHITE);
        } else {
            DrawText(title, (width - MeasureText(title, 48)) / 2, height / 2 - 24, 48, RAYWHITE);
        }
    }

    std::string status = nexo
                                   ? (m_assetsLoaded
                                          ? nexo->text("splash.preparing")
                                          : nexo->text("splash.loading_resource"))
                                   : std::string{};
    if (nexo) replaceToken(status, "{resource}", nexo->assets().loadingResource());
    const Color statusColor{.r = 190, .g = 199, .b = 214, .a = 255};
    if (font) {
        const Vector2 size = MeasureTextEx(*font, status.c_str(), 22, 1);
        DrawTextEx(*font, status.c_str(), {.x = (width - size.x) * .5f, .y = height - 38.f}, 22, 1, statusColor);
    } else {
        DrawText(status.c_str(), (width - MeasureText(status.c_str(), 18)) / 2, height - 34, 18, statusColor);
    }
}
