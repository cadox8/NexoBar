//
// Created by cadox8 on 23/10/2026.
//

#include "NexoBar.h"

#include "license/license.h"
#include "database/hospitality/HospitalityDatabase.h"
#include "manager/ScreenManager.h"
#include "ui/screen/splash/MessageScreen.h"
#include "ui/screen/sales/ProductScreen.h"
#include "ui/screen/splash/SplashScreen.h"
#include "log/Log.h"

#include <cstdarg>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>
#include <raylib.h>
#include <rlgl.h>
#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#endif

NexoBar* NexoBarImp::m_nexo = nullptr;

NexoBar::~NexoBar() = default;

namespace {
    void raylibLogCallback(const int logLevel, const char* text, va_list args) {
        char message[2048]{};
        std::vsnprintf(message, sizeof(message), text, args);

        if (logLevel == LOG_WARNING) {
            NEXO_WARN(LogModule::Core) << message;
        } else if (logLevel == LOG_ERROR || logLevel == LOG_FATAL) {
            NEXO_ERROR(LogModule::Core) << message;
        }
    }

    std::filesystem::path executableResourceDirectory() {
#ifdef _WIN32
        std::vector<wchar_t> executablePath(32768);
        const DWORD length = GetModuleFileNameW(nullptr, executablePath.data(), static_cast<DWORD>(executablePath.size()));
        if (length > 0 && length < executablePath.size())
            return std::filesystem::path(executablePath.data()).parent_path() / L"resources";
#endif
        return std::filesystem::current_path() / "resources";
    }

    std::filesystem::path languageCatalogDirectory() {
        const auto local = executableResourceDirectory() / "languages";
        if (std::filesystem::is_directory(local))
            return local;
        const auto source = std::filesystem::current_path() / "NexoBar" / "resources" / "languages";
        if (std::filesystem::is_directory(source))
            return source;
        return local;
    }
}

void NexoBar::run() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    try {
        init();
    } catch (const std::exception& error) {
        addStartupError(text("errors.exception_prefix", "Unexpected error: ") + error.what());
        NEXO_ERROR(LogModule::Core) << "Application initialization failed: " << error.what();
    } catch (...) {
        addStartupError(text("errors.unknown_startup", "Unknown error during application startup."));
        NEXO_ERROR(LogModule::Core) << startupErrors().back();
    }

    SetTraceLogCallback(raylibLogCallback);
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIGHDPI);
    InitWindow(960, 600, "NexoBar");
    m_windowInitialized = IsWindowReady();
    if (!m_windowInitialized) {
        NEXO_ERROR(LogModule::Core) << "Raylib could not initialize the application window";
        shutdown();
        return;
    }

    try {
        m_defaultFont = m_assets.getFont("default");
    } catch (const std::exception& error) {
        NEXO_ERROR(LogModule::Core) << "Could not load the default font: " << error.what();
        addStartupError(text("errors.resources_load", "Could not load application resources: ") + error.what());
        m_defaultFont = FontPtr(new Font(GetFontDefault()), [](Font* font) { delete font; });
    }

    const auto iconPath = executableResourceDirectory() / "logo" / "NexoBar_icon.png";
    Image icon = LoadImage(iconPath.string().c_str());
    if (icon.data) {
        ImageAlphaCrop(&icon, 0.1f);
        SetWindowIcon(icon);
        UnloadImage(icon);
    } else {
        NEXO_WARN(LogModule::Core) << "Could not load the application icon: " << iconPath.string();
    }

    InitAudioDevice();
    m_audioInitialized = IsAudioDeviceReady();
    if (!m_audioInitialized)
        addStartupWarning(text("warnings.no_audio", "Audio device could not be initialized."));

    auto productState = std::make_shared<ProductState>(m_database.get());
    m_screens.registerScreen("catalog", std::make_shared<ProductScreen>(productState));
    const auto makeMessageScreen = [this](const std::string_view titleKey, const std::string_view titleFallback) {
        return std::make_shared<MessageScreen>(
            text(titleKey, titleFallback), m_startupErrors, m_startupWarnings,
            text("errors.error_prefix", "Error: "), text("warnings.warning_prefix", "Warning: "), m_defaultFont);
    };
    m_screens.registerScreen("errors", makeMessageScreen("errors.startup_title", "Startup errors"));
    m_screens.registerScreen("splash", std::make_shared<SplashScreen>());
    m_screens.changeScreen("splash");

    auto previousFrameStart = std::chrono::steady_clock::now();
    while (!WindowShouldClose()) {
        const auto frameStart = std::chrono::steady_clock::now();
        const float deltaSeconds = std::chrono::duration<float>(frameStart - previousFrameStart).count();
        previousFrameStart = frameStart;
        try {
            m_screens.update(deltaSeconds);
        } catch (const std::exception& error) {
            addStartupError(text("errors.exception_prefix", "Unexpected error: ") + error.what());
            NEXO_ERROR(LogModule::Core) << "Screen update failed: " << error.what();
            m_screens.set(makeMessageScreen("errors.runtime_title", "Screen error"));
        } catch (...) {
            addStartupError(text("errors.unknown_update", "Unknown error while updating the screen."));
            NEXO_ERROR(LogModule::Core) << startupErrors().back();
            m_screens.set(makeMessageScreen("errors.runtime_title", "Screen error"));
        }
        bool drawFailed = false;
        BeginDrawing();
        ClearBackground(Color{18, 23, 32, 255});
        try {
            m_screens.draw();
        } catch (const std::exception& error) {
            addStartupError(text("errors.exception_prefix", "Unexpected error: ") + error.what());
            NEXO_ERROR(LogModule::Core) << "Screen drawing failed: " << error.what();
            const std::string label = text("errors.draw_failed", "Screen drawing failed.");
            if (m_defaultFont) {
                DrawTextEx(*m_defaultFont, label.c_str(), {24, 24}, 22, 1, RED);
                DrawTextEx(*m_defaultFont, error.what(), {24, 58}, 18, 1, RAYWHITE);
            } else {
                DrawText(label.c_str(), 24, 24, 20, RED);
                DrawText(error.what(), 24, 54, 16, RAYWHITE);
            }
            drawFailed = true;
        } catch (...) {
            addStartupError(text("errors.unknown_draw", "Unknown error while drawing the screen."));
            NEXO_ERROR(LogModule::Core) << "Unknown error while drawing the screen";
            const std::string label = text("errors.unknown_draw", "Unknown error while drawing the screen.");
            if (m_defaultFont)
                DrawTextEx(*m_defaultFont, label.c_str(), {24, 24}, 22, 1, RED);
            else
                DrawText(label.c_str(), 24, 24, 20, RED);
            drawFailed = true;
        }
        rlDrawRenderBatchActive();
        SwapScreenBuffer();
        PollInputEvents();
        if (drawFailed)
            m_screens.set(makeMessageScreen("errors.runtime_title", "Screen error"));
        constexpr auto frameDuration = std::chrono::duration<double>(1.0 / 60.0);
        std::this_thread::sleep_until(
            frameStart + std::chrono::duration_cast<std::chrono::steady_clock::duration>(frameDuration));
    }

    shutdown();
}

void NexoBar::init() {
    try {
        const auto logoPath = executableResourceDirectory() / "logo" / "NexoBar_icon.png";
        if (!std::filesystem::is_regular_file(logoPath))
            throw std::runtime_error("NexoBar icon not found: " + logoPath.string());
        m_assets.registerTexture("nexobar-icon", logoPath.string());
        m_assets.registerTexture("icon-edit", "ui/edit.png");
        m_assets.registerTexture("icon-delete", "ui/delete.png");
        m_assets.registerTexture("icon-cart", "ui/cart.png");
        m_assets.registerFont("default", "inter/static/Inter_18pt-Regular.ttf", 96);
        m_assets.discoverResources();
        m_i18n = std::make_unique<platform::I18n>(languageCatalogDirectory());
    } catch (const std::exception& error) {
        NEXO_ERROR(LogModule::Core) << "Can not prepare application resources: " << error.what();
        addStartupError(text("errors.resources_load", "Could not load application resources: ") + error.what());
        return;
    }

    try {
        m_database = nexobar::openHospitalityDatabase("nexobar.db", *m_i18n);
        m_backups = std::make_unique<Nearon::AutomaticBackup>(*m_database, m_database->path().parent_path() / "backups",
                                                              Nearon::AutomaticBackup::Options{std::chrono::hours(24), std::chrono::days(30)},
                                                              [](const std::string& error) { NEXO_ERROR(LogModule::Database) << "Automatic backup failed: " << error; });
        m_backups->start();
    } catch (const std::exception& error) {
        NEXO_ERROR(LogModule::Database) << "Can not open database: " << error.what();
        addStartupError(text("errors.database_start", "Could not start the database: ") + error.what());
        return;
    }

    const License license;

    if (m_database->count<nexobar::Business>() == 0) {
        NEXO_WWARN(LogModule::Core) << "No business is configured; continuing in product test mode";
        addStartupWarning(text("warnings.no_business", "No business is configured; starting product test mode."));
        return;
    }

    const auto business = m_database->query<nexobar::Business>().first();
    if (!business) {
        addStartupError(text("errors.business_read", "Could not read the configured business."));
        return;
    } else {
        m_business = *business;
    }

    const License::LicenseType type = license.validate(business->license);
    if (type == License::LicenseType::Invalid) {
        NEXO_ERROR(LogModule::Core) << "Invalid License";
        addStartupError(text("errors.invalid_license", "The configured license is invalid."));
    }
}

void NexoBar::shutdown() {
    m_screens.reset();
    m_backups.reset();
    m_database.reset();
    m_i18n.reset();
    m_defaultFont.reset();
    m_assets.clear();
    if (m_audioInitialized) {
        CloseAudioDevice();
        m_audioInitialized = false;
    }
    if (m_windowInitialized) {
        CloseWindow();
        m_windowInitialized = false;
    }
}

void NexoBar::addStartupError(std::string message) {
    if (!message.empty())
        m_startupErrors.push_back(std::move(message));
}

void NexoBar::addStartupWarning(std::string message) {
    if (!message.empty())
        m_startupWarnings.push_back(std::move(message));
}

std::string NexoBar::text(const std::string_view key, const std::string_view fallback) const {
    if (m_i18n) {
        const std::string translated = m_i18n->get(key);
        if (translated != key)
            return translated;
    }
    return std::string(fallback);
}
