//
// Created by cadox8 on 23/10/2026.
//
#pragma once

#include "database/Database.h"
#include "database/AutomaticBackup.h"
#include "i18n/I18n.h"
#include "manager/AssetManager.h"
#include "manager/ScreenManager.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "database/models/Business.h"

class NexoBar {
public:
    NexoBar() = default;

    ~NexoBar();

    void init();

    void run();

    void shutdown();

    const std::shared_ptr<Nearon::Database>& database() const { return m_database; }
    const platform::I18n* i18n() const noexcept { return m_i18n.get(); }

    std::string text(std::string_view key, std::string_view fallback) const;

    AssetManager& assets() noexcept { return m_assets; }
    FontPtr defaultFont() const noexcept { return m_defaultFont; }
    ScreenManager& screens() noexcept { return m_screens; }
    const std::vector<std::string>& startupErrors() const noexcept { return m_startupErrors; }
    const std::vector<std::string>& startupWarnings() const noexcept { return m_startupWarnings; }

    void addStartupError(std::string message);

    void addStartupWarning(std::string message);

    const nexobar::Business& business() { return m_business; }
    void setBusiness(nexobar::Business business) { m_business = std::move(business); }

    bool ticketDebugMode() const noexcept {
#ifndef NDEBUG
        return m_ticketDebugMode;
#else
        return false;
#endif
    }

    void setTicketDebugMode(bool enabled) noexcept {
#ifndef NDEBUG
        m_ticketDebugMode = enabled;
#else
        (void) enabled;
#endif
    }

private:
    std::unique_ptr<platform::I18n> m_i18n;
    std::shared_ptr<Nearon::Database> m_database;
    std::unique_ptr<Nearon::AutomaticBackup> m_backups;
    AssetManager m_assets;
    FontPtr m_defaultFont;
    ScreenManager m_screens;
    std::vector<std::string> m_startupErrors;
    std::vector<std::string> m_startupWarnings;
    bool m_windowInitialized = false;
    bool m_audioInitialized = false;
#ifndef NDEBUG
    bool m_ticketDebugMode = true;
#else
    bool m_ticketDebugMode = false;
#endif

    nexobar::Business m_business;
};

class NexoBarImp {
public:
    NexoBarImp() = default;

    virtual ~NexoBarImp() = default;

    static void set(NexoBar* api) { m_nexo = api; }
    static NexoBar* get() { return m_nexo; }
    explicit operator NexoBar*() const { return m_nexo; }
    NexoBar* operator->() const { return m_nexo; }
    explicit operator bool() const { return m_nexo != nullptr; }

private:
    static NexoBar* m_nexo;
};
