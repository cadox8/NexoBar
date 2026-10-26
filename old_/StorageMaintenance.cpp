#include "../StorageMaintenance.h"

#include <sqlite3.h>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace tpv {
namespace {

class SqliteHandle {
public:
    explicit SqliteHandle(sqlite3* db = nullptr) : db_(db) {}
    ~SqliteHandle() { if (db_) sqlite3_close_v2(db_); }
    SqliteHandle(const SqliteHandle&) = delete;
    SqliteHandle& operator=(const SqliteHandle&) = delete;
    sqlite3* get() const { return db_; }
    sqlite3** out() { return &db_; }
private:
    sqlite3* db_{};
};

void openDatabase(SqliteHandle& handle, const std::string& path, int flags) {
    const int rc = sqlite3_open_v2(path.c_str(), handle.out(), flags, nullptr);
    if (rc != SQLITE_OK) {
        const std::string message = handle.get() ? sqlite3_errmsg(handle.get()) : "sin detalle";
        throw std::runtime_error("No se pudo abrir '" + path + "': " + message);
    }
    sqlite3_busy_timeout(handle.get(), 5000);
}

void runBackup(sqlite3* destination, sqlite3* source) {
    sqlite3_backup* operation = sqlite3_backup_init(destination, "main", source, "main");
    if (!operation) {
        throw std::runtime_error(std::string("No se pudo iniciar la copia SQLite: ") + sqlite3_errmsg(destination));
    }

    int rc = SQLITE_OK;
    do {
        rc = sqlite3_backup_step(operation, 256);
        if (rc == SQLITE_BUSY || rc == SQLITE_LOCKED) sqlite3_sleep(50);
    } while (rc == SQLITE_OK || rc == SQLITE_BUSY || rc == SQLITE_LOCKED);

    const int finishRc = sqlite3_backup_finish(operation);
    if (rc != SQLITE_DONE) {
        throw std::runtime_error(std::string("Falló la copia SQLite: ") + sqlite3_errmsg(destination));
    }
    if (finishRc != SQLITE_OK) {
        throw std::runtime_error(std::string("No se pudo finalizar la copia SQLite: ") + sqlite3_errmsg(destination));
    }
}

} // namespace

IntegrityReport StorageMaintenance::verify(const std::string& databasePath) {
    IntegrityReport report;
    SqliteHandle db;
    openDatabase(db, databasePath, SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX);

    sqlite3_stmt* statement = nullptr;
    int rc = sqlite3_prepare_v2(db.get(), "PRAGMA integrity_check;", -1, &statement, nullptr);
    if (rc != SQLITE_OK) throw std::runtime_error(std::string("No se pudo ejecutar integrity_check: ") + sqlite3_errmsg(db.get()));

    bool integrityOk = true;
    while ((rc = sqlite3_step(statement)) == SQLITE_ROW) {
        const auto* value = sqlite3_column_text(statement, 0);
        const std::string message = value ? reinterpret_cast<const char*>(value) : "resultado vacío";
        report.messages.push_back("integrity_check: " + message);
        if (message != "ok") integrityOk = false;
    }
    sqlite3_finalize(statement);
    if (rc != SQLITE_DONE) throw std::runtime_error(std::string("Error leyendo integrity_check: ") + sqlite3_errmsg(db.get()));

    statement = nullptr;
    rc = sqlite3_prepare_v2(db.get(), "PRAGMA foreign_key_check;", -1, &statement, nullptr);
    if (rc != SQLITE_OK) throw std::runtime_error(std::string("No se pudo ejecutar foreign_key_check: ") + sqlite3_errmsg(db.get()));

    bool foreignKeysOk = true;
    while ((rc = sqlite3_step(statement)) == SQLITE_ROW) {
        foreignKeysOk = false;
        const auto* table = sqlite3_column_text(statement, 0);
        const auto rowid = sqlite3_column_int64(statement, 1);
        const auto* parent = sqlite3_column_text(statement, 2);
        report.messages.push_back("foreign_key_check: tabla=" + std::string(table ? reinterpret_cast<const char*>(table) : "?") +
                                  ", rowid=" + std::to_string(rowid) +
                                  ", referencia=" + (parent ? reinterpret_cast<const char*>(parent) : "?"));
    }
    sqlite3_finalize(statement);
    if (rc != SQLITE_DONE) throw std::runtime_error(std::string("Error leyendo foreign_key_check: ") + sqlite3_errmsg(db.get()));
    if (foreignKeysOk) report.messages.push_back("foreign_key_check: sin infracciones");

    report.ok = integrityOk && foreignKeysOk;
    return report;
}

void StorageMaintenance::backup(const std::string& sourcePath, const std::string& backupPath) {
    if (std::filesystem::absolute(sourcePath).lexically_normal() == std::filesystem::absolute(backupPath).lexically_normal()) {
        throw std::runtime_error("El origen y el destino de la copia no pueden ser el mismo archivo.");
    }
    if (!std::filesystem::exists(sourcePath)) throw std::runtime_error("No existe la base de datos de origen: " + sourcePath);

    if (std::filesystem::exists(backupPath)) std::filesystem::remove(backupPath);
    SqliteHandle source;
    SqliteHandle destination;
    openDatabase(source, sourcePath, SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX);
    openDatabase(destination, backupPath, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX);
    runBackup(destination.get(), source.get());

    const auto report = verify(backupPath);
    if (!report.ok) throw std::runtime_error("La copia creada no ha superado la comprobación de integridad.");
}

void StorageMaintenance::restore(const std::string& backupPath, const std::string& destinationPath) {
    if (!std::filesystem::exists(backupPath)) throw std::runtime_error("No existe la copia: " + backupPath);
    if (std::filesystem::absolute(backupPath).lexically_normal() == std::filesystem::absolute(destinationPath).lexically_normal()) {
        throw std::runtime_error("La ruta de destino debe ser distinta de la copia de origen.");
    }
    const auto sourceReport = verify(backupPath);
    if (!sourceReport.ok) throw std::runtime_error("La copia de origen no ha superado la comprobación de integridad.");

    if (std::filesystem::exists(destinationPath)) std::filesystem::remove(destinationPath);
    SqliteHandle source;
    SqliteHandle destination;
    openDatabase(source, backupPath, SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX);
    openDatabase(destination, destinationPath, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX);
    runBackup(destination.get(), source.get());

    const auto destinationReport = verify(destinationPath);
    if (!destinationReport.ok) throw std::runtime_error("La base restaurada no ha superado la comprobación de integridad.");
}

} // namespace tpv
