//
// Created by cadox8 on 23/10/2026.
//
#pragma once
#include "Version.h"
#include "database/Database.h"
#include "database/AutomaticBackup.h"
#include <memory>
#include <exception>
#include <filesystem>
#include <string>
#include <string_view>


class Management {
public:
    Management() : m_version(0, 2, 0) {}

    ~Management() = default;

    void init();

    void initializeDefaults(const std::filesystem::path& databasePath);

    void shutdown();

    [[nodiscard]] const Version& version() const { return m_version; }
    [[nodiscard]] std::string userError(const std::exception& error) const;
    [[nodiscard]] std::string text(std::string_view key) const;

private:
    Version m_version;
    std::shared_ptr<Nearon::Database> m_database;
    std::unique_ptr<Nearon::AutomaticBackup> m_backups;
};

class ManagementImp {
public:
    ManagementImp() = default;

    virtual ~ManagementImp() = default;

    static void set(Management* api) { m_nexo = api; }
    explicit operator Management*() const { return m_nexo; }
    Management* operator->() const { return m_nexo; }
    explicit operator bool() const { return m_nexo != nullptr; }

private:
    static Management* m_nexo;
};
