#include "../StorageMaintenance.h"

#include <filesystem>
#include <iostream>
#include <sqlite3.h>
#include <stdexcept>
#include <string>

namespace {
void usage() {
    std::cout << "Uso:\n"
              << "  tpv_maintenance check <base.db>\n"
              << "  tpv_maintenance backup <base.db> <copia.db>\n"
              << "  tpv_maintenance restore <copia.db> <destino.db>\n"
              << "  tpv_maintenance self-test\n";
}

void check(const std::string& path) {
    const auto report = tpv::StorageMaintenance::verify(path);
    for (const auto& message : report.messages) std::cout << message << '\n';
    if (!report.ok) throw std::runtime_error("La base de datos no ha superado la comprobación.");
    std::cout << "Resultado: OK\n";
}

void selfTest() {
    const auto base = std::filesystem::temp_directory_path() / "tpv_maintenance_selftest.db";
    const auto backup = std::filesystem::temp_directory_path() / "tpv_maintenance_selftest_backup.db";
    const auto restored = std::filesystem::temp_directory_path() / "tpv_maintenance_selftest_restored.db";
    for (const auto& path : {base, backup, restored}) {
        if (std::filesystem::exists(path)) std::filesystem::remove(path);
    }

    // Crear una base mínima usando SQLite directamente.
    // El test valida backup/restore/integridad, no la conformidad fiscal.
    sqlite3* db = nullptr;
    if (sqlite3_open(base.string().c_str(), &db) != SQLITE_OK) {
        const std::string error = db ? sqlite3_errmsg(db) : "sin detalle";
        if (db) sqlite3_close(db);
        throw std::runtime_error("No se pudo crear la base de prueba: " + error);
    }
    char* error = nullptr;
    const int rc = sqlite3_exec(db,
        "PRAGMA foreign_keys=ON; CREATE TABLE sample(id INTEGER PRIMARY KEY, value TEXT NOT NULL);"
        "INSERT INTO sample(value) VALUES('prueba de copia');",
        nullptr, nullptr, &error);
    if (rc != SQLITE_OK) {
        const std::string message = error ? error : sqlite3_errmsg(db);
        sqlite3_free(error);
        sqlite3_close(db);
        throw std::runtime_error("No se pudo preparar la base de prueba: " + message);
    }
    sqlite3_close(db);

    tpv::StorageMaintenance::backup(base.string(), backup.string());
    tpv::StorageMaintenance::restore(backup.string(), restored.string());
    check(restored.string());
    std::cout << "Self-test de backup/restauración: OK\n";
    for (const auto& path : {base, backup, restored}) {
        if (std::filesystem::exists(path)) std::filesystem::remove(path);
    }
}
} // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string(argv[1]) == "self-test") {
            selfTest();
            return 0;
        }
        if (argc == 3 && std::string(argv[1]) == "check") {
            check(argv[2]);
            return 0;
        }
        if (argc == 4 && std::string(argv[1]) == "backup") {
            tpv::StorageMaintenance::backup(argv[2], argv[3]);
            std::cout << "Copia creada y verificada: " << argv[3] << '\n';
            return 0;
        }
        if (argc == 4 && std::string(argv[1]) == "restore") {
            tpv::StorageMaintenance::restore(argv[2], argv[3]);
            std::cout << "Restauración creada y verificada: " << argv[3] << '\n';
            return 0;
        }
        usage();
        return 2;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << '\n';
        return 1;
    }
}
