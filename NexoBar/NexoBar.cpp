//
// Created by cadox8 on 23/10/2026.
//

#include "NexoBar.h"

#include "license/license.h"
#include "database/hospitality/HospitalityDatabase.h"
#include "manager/ScreenManager.h"
#include "ui/screen/splash/MessageScreen.h"
#include "ui/screen/main/MainScreen.h"
#include "ui/screen/auth/LoginScreen.h"
#include "ui/screen/splash/LoadingScreen.h"
#include "ui/screen/splash/SplashScreen.h"
#include "log/Log.h"
#include "EmbeddedLanguages.h"
#include "reporting/DailyCloseReceipt.h"

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
}

void NexoBar::run() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    try {
        init();
    } catch (const std::exception& error) {
        addStartupError(userError(error, "errors.startup_failed"));
        NEXO_ERROR(LogModule::Core) << "Application initialization failed: " << error.what();
    } catch (...) {
        addStartupError(text("errors.unknown_startup"));
        NEXO_ERROR(LogModule::Core) << startupErrors().back();
    }

    SetTraceLogCallback(raylibLogCallback);
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIGHDPI);
    const std::string applicationName = text("app.name");
    InitWindow(960, 600, applicationName.c_str());
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
        addStartupError(userError(error, "errors.resources_load"));
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
        addStartupWarning(text("warnings.no_audio"));

    const auto makeMessageScreen = [this](const std::string_view titleKey) {
        return std::make_shared<MessageScreen>(
            text(titleKey), m_startupErrors, m_startupWarnings, text("warnings.warning_prefix"), m_defaultFont);
    };
    m_screens.registerScreen("errors", makeMessageScreen("errors.startup_title"));
    m_screens.registerScreen("loading", std::make_shared<LoadingScreen>());
    m_screens.registerScreen("login", std::make_shared<LoginScreen>([this](const std::int64_t userId) { openSession(userId); }));
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
            addStartupError(userError(error, "errors.screen_update_failed"));
            NEXO_ERROR(LogModule::Core) << "Screen update failed: " << error.what();
            m_screens.set(makeMessageScreen("errors.runtime_title"));
        } catch (...) {
            addStartupError(text("errors.unknown_update"));
            NEXO_ERROR(LogModule::Core) << startupErrors().back();
            m_screens.set(makeMessageScreen("errors.runtime_title"));
        }
        bool drawFailed = false;
        BeginDrawing();
        ClearBackground(Color{18, 23, 32, 255});
        try {
            m_screens.draw();
        } catch (const std::exception& error) {
            const std::string message = userError(error, "errors.screen_draw_failed");
            addStartupError(message);
            NEXO_ERROR(LogModule::Core) << "Screen drawing failed: " << error.what();
            if (m_defaultFont) {
                DrawTextEx(*m_defaultFont, message.c_str(), {24, 24}, 22, 1, RED);
            } else {
                DrawText(message.c_str(), 24, 24, 20, RED);
            }
            drawFailed = true;
        } catch (...) {
            addStartupError(text("errors.unknown_draw"));
            NEXO_ERROR(LogModule::Core) << "Unknown error while drawing the screen";
            const std::string label = text("errors.unknown_draw");
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
            m_screens.set(makeMessageScreen("errors.runtime_title"));
        constexpr auto frameDuration = std::chrono::duration<double>(1.0 / 60.0);
        std::this_thread::sleep_until(
            frameStart + std::chrono::duration_cast<std::chrono::steady_clock::duration>(frameDuration));
    }

    closeCurrentBillingDay();
    shutdown();
}

void NexoBar::showLogin() {
    if (!canEnterSession()) {
        showStartupErrors();
        return;
    }
    m_screens.registerScreen("login", std::make_shared<LoginScreen>(
                                 [this](const std::int64_t userId) { openSession(userId); }));
    m_screens.changeScreen("login");
}

void NexoBar::openSession(const std::int64_t userId) {
    if (!canEnterSession()) {
        showStartupErrors();
        return;
    }
    m_currentUserId = nexobar::UserId{userId};
    auto state = std::make_shared<ProductState>(m_database.get(), userId);
    m_screens.registerScreen("main", std::make_shared<MainScreen>(state, [this] { logout(); }));
    m_screens.changeScreen("main");
}

void NexoBar::logout() { showLogin(); }

void NexoBar::showStartupErrors() {
    m_screens.registerScreen("errors", std::make_shared<MessageScreen>(
        text("errors.startup_title"), m_startupErrors, m_startupWarnings,
        text("warnings.warning_prefix"), m_defaultFont));
    m_screens.changeScreen("errors");
}

void NexoBar::closeCurrentBillingDay() {
    if (!m_database || !m_i18n || m_business.tradeName.empty()) return;
    std::string error;
    const bool printed = DailyCloseReceipt::print(
        *m_database, boost::gregorian::day_clock::local_day(),
        m_business.tradeName, *m_i18n, true, m_currentUserId, error);
    if (!printed)
        NEXO_ERROR(LogModule::Printer) << "Could not print billing close before exit: " << error;
}

void NexoBar::init() {
    m_businessConfigured = false;
    try {
        m_i18n = std::make_unique<platform::I18n>(embeddedLanguageCatalogs());
        const auto logoPath = executableResourceDirectory() / "logo" / "NexoBar_icon.png";
        if (!std::filesystem::is_regular_file(logoPath))
            throw std::runtime_error(text("errors.icon_missing") + logoPath.string());
        m_assets.registerTexture("nexobar-icon", logoPath.string());
        m_assets.registerTexture("icon-edit", "ui/edit.svg");
        m_assets.registerTexture("icon-delete", "ui/delete.svg");
        m_assets.registerTexture("icon-cart", "ui/cart.svg");
        m_assets.registerTexture("icon-reports", "ui/reports.svg");
        m_assets.registerTexture("icon-settings", "ui/settings.svg");
        m_assets.registerTexture("icon-logout", "ui/logout.svg");
        m_assets.registerFont("default", "static/Inter_18pt-Regular.ttf", 96);
        m_assets.discoverResources();
    } catch (const std::exception& error) {
        NEXO_ERROR(LogModule::Core) << "Can not prepare application resources: " << error.what();
        addStartupError(userError(error, "errors.resources_load"));
        return;
    }

    try {
        m_database = nexobar::openHospitalityDatabase(
            "nexobar.db", [this](const std::string_view key) { return m_i18n->get(key); });
        m_backups = std::make_unique<Nearon::AutomaticBackup>(*m_database, m_database->path().parent_path() / "backups",
                                                              Nearon::AutomaticBackup::Options{std::chrono::hours(24), std::chrono::days(30)},
                                                              [](const std::string& error) { NEXO_ERROR(LogModule::Database) << "Automatic backup failed: " << error; });
        m_backups->start();
    } catch (const std::exception& error) {
        NEXO_ERROR(LogModule::Database) << "Can not open database: " << error.what();
        addStartupError(userError(error, "errors.database_start"));
        return;
    }

    const License license;

    if (m_database->count<nexobar::Business>() == 0) {
        NEXO_ERROR(LogModule::Core) << "No business is configured; session entry is blocked";
        addStartupError(text("errors.no_business"));
        return;
    }

    const auto business = m_database->query<nexobar::Business>().first();
    if (!business) {
        addStartupError(text("errors.business_read"));
        return;
    } else {
        m_business = *business;
        m_businessConfigured = true;
    }

    const License::LicenseType type = license.validate(business->license);
    if (type == License::LicenseType::Invalid) {
        NEXO_ERROR(LogModule::Core) << "Invalid License";
        addStartupError(text("errors.invalid_license"));
    }
}

void NexoBar::shutdown() {
    m_screens.reset();
    m_backups.reset();
    m_database.reset();
    m_i18n.reset();
    m_defaultFont.reset();
    m_assets.clear();
    m_businessConfigured = false;
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
    if (!message.empty() &&
        std::ranges::find(m_startupErrors, message) == m_startupErrors.end())
        m_startupErrors.push_back(std::move(message));
}

void NexoBar::addStartupWarning(std::string message) {
    if (!message.empty())
        m_startupWarnings.push_back(std::move(message));
}

std::string NexoBar::text(const std::string_view key) const {
    if (m_i18n) {
        const std::string translated = m_i18n->get(key);
        if (translated != key)
            return translated;
    }
    return std::string(key);
}

std::string NexoBar::userError(const std::exception& error,
                               const std::string_view fallbackKey) const {
    if (const auto* hospitality = dynamic_cast<const nexobar::HospitalityError*>(&error))
        return text(hospitality->translationKey());

    const std::string detail = error.what();
    if (detail.starts_with("errors.")) {
        const std::string translated = text(detail);
        if (translated != detail)
            return translated;
    }

    // Layout and screen infrastructure messages are useful to developers but
    // should not leak implementation details (or untranslated English) to users.
    if (detail.find("Layout") != std::string::npos ||
        detail.find("layout") != std::string::npos ||
        detail.find("Screen region") != std::string::npos)
        return text("errors.layout_invalid");

    return text(fallbackKey);
}
