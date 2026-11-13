#include "AutomaticBackup.h"

#include <sqlite3.h>

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace Nearon {
    AutomaticBackup::AutomaticBackup(Connection& source, std::filesystem::path directory, Options options,
                                     std::function<void(const std::string&)> onError)
        : source_(source), directory_(std::move(directory)), options_(options), onError_(std::move(onError)) {
        if (options_.interval <= std::chrono::hours::zero()) throw std::invalid_argument("Backup interval must be positive");
        if (options_.retention <= std::chrono::days::zero()) throw std::invalid_argument("Backup retention must be positive");
        if (source_.path() == ":memory:") throw std::invalid_argument("Automatic backups require a file database");
    }

    AutomaticBackup::~AutomaticBackup() { stop(); }

    void AutomaticBackup::start() {
        std::lock_guard lock(mutex_);
        if (started_) return;
        stopping_ = false;
        started_ = true;
        worker_ = std::thread([this] { run(); });
    }

    void AutomaticBackup::stop() {
        {
            std::lock_guard lock(mutex_);
            if (!started_) return;
            stopping_ = true;
        }
        cv_.notify_all();
        if (worker_.joinable()) worker_.join();
        std::lock_guard lock(mutex_);
        started_ = false;
    }

    void AutomaticBackup::backupNow() {
        std::filesystem::create_directories(directory_);
        const auto now = std::chrono::system_clock::now();
        const auto stamp = std::chrono::system_clock::to_time_t(now);
        const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
        std::tm utc{};
#ifdef _WIN32
        gmtime_s(&utc, &stamp);
#else
        gmtime_r(&stamp, &utc);
#endif
        std::ostringstream filename;
        filename << "nexobar_" << std::put_time(&utc, "%Y%m%d_%H%M%S") << '_'
                 << std::setw(3) << std::setfill('0') << milliseconds;
        auto destination = directory_ / (filename.str() + ".db");
        for (unsigned suffix = 1; std::filesystem::exists(destination); ++suffix)
            destination = directory_ / (filename.str() + "_" + std::to_string(suffix) + ".db");
        auto temporary = destination;
        temporary += ".tmp";

        std::lock_guard dbLock(source_.mutex());
        sqlite3* target = nullptr;
        const auto tempUtf8 = temporary.u8string();
        const std::string tempPath(tempUtf8.begin(), tempUtf8.end());
        int rc = sqlite3_open_v2(tempPath.c_str(), &target, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr);
        if (rc != SQLITE_OK) {
            const std::string message = target ? sqlite3_errmsg(target) : sqlite3_errstr(rc);
            sqlite3_close_v2(target);
            throw std::runtime_error("Could not create backup: " + message);
        }
        sqlite3_backup* backup = sqlite3_backup_init(target, "main", source_.handle(), "main");
        if (!backup) {
            const std::string message = sqlite3_errmsg(target);
            sqlite3_close_v2(target);
            std::filesystem::remove(temporary);
            throw std::runtime_error("Could not initialize backup: " + message);
        }
        do {
            rc = sqlite3_backup_step(backup, -1);
            if (rc == SQLITE_BUSY || rc == SQLITE_LOCKED) sqlite3_sleep(50);
        } while (rc == SQLITE_BUSY || rc == SQLITE_LOCKED);
        const int finishRc = sqlite3_backup_finish(backup);
        if (rc != SQLITE_DONE || finishRc != SQLITE_OK) {
            const std::string message = sqlite3_errmsg(target);
            sqlite3_close_v2(target);
            std::filesystem::remove(temporary);
            throw std::runtime_error("Could not complete backup: " + message);
        }
        rc = sqlite3_close(target);
        if (rc != SQLITE_OK) {
            std::filesystem::remove(temporary);
            throw std::runtime_error("Could not close backup database");
        }
        std::error_code ec;
        std::filesystem::rename(temporary, destination, ec);
        if (ec) {
            std::filesystem::remove(temporary);
            throw std::runtime_error("Could not publish backup: " + ec.message());
        }
        prune();
    }

    void AutomaticBackup::prune() {
        const auto cutoff = std::filesystem::file_time_type::clock::now() - options_.retention;
        for (const auto& entry : std::filesystem::directory_iterator(directory_)) {
            if (!entry.is_regular_file()) continue;
            const auto name = entry.path().filename().string();
            if (name.rfind("nexobar_", 0) != 0 || entry.path().extension() != ".db") continue;
            std::error_code ec;
            if (entry.last_write_time(ec) < cutoff && !ec) std::filesystem::remove(entry.path(), ec);
        }
    }

    void AutomaticBackup::run() {
        while (true) {
            try { backupNow(); }
            catch (const std::exception& error) { if (onError_) onError_(error.what()); }
            std::unique_lock lock(mutex_);
            if (cv_.wait_for(lock, options_.interval, [this] { return stopping_; })) return;
        }
    }
}
