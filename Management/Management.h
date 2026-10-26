//
// Created by cadox8 on 23/10/2026.
//
#pragma once
#include "Version.h"
#include "database/Database.h"
#include "database/AutomaticBackup.h"
#include <memory>


class Management {
public:
    Management() : m_version(1, 0, 0) {}

    ~Management() = default;

    void init();

    void shutdown();

    [[nodiscard]] const Version& version() const { return m_version; }

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
    static Management* get() { return m_nexo; }
    explicit operator Management*() const { return m_nexo; }
    Management* operator->() const { return m_nexo; }
    explicit operator bool() const { return m_nexo != nullptr; }

private:
    static Management* m_nexo;
};
