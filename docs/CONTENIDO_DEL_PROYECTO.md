# Contenido del proyecto TPV Backend

Documento móvil de referencia. Los bloques muestran los archivos fuente actuales; el módulo fiscal completo no está implementado.


---

## `../README.md`

```markdown
# TPV backend C++20 (prototipo operativo)

Backend local para Windows 11: SQLite, artículos, ventas, importe libre, pagos, caja, ticket ESC/POS RAW y pulso de cajón.

## Importante sobre Hacienda

Este proyecto es un prototipo operativo, **no es un sistema fiscal validado ni se declara conforme al RD 1007/2023, a la Orden
HAC/1177/2024 ni a Veri*Factu**. No implementa registro fiscal encadenado, firma, registro de eventos fiscal, QR fiscal,
exportación fiscal ni declaración responsable. No debe usarse como software de facturación real hasta completar y verificar esos
requisitos con la documentación vigente y asesoramiento especializado.

Los tickets de este proyecto son tickets operativos simplificados para pruebas. La impresión de un ticket no equivale por sí sola
a expedir una factura fiscal.

## Dependencias

- C++20 y CMake 3.24+
- SQLite3 con paquete CMake `SQLite3`
- Windows 11 para impresión RAW con Winspool

Ejemplo con vcpkg:

```powershell
vcpkg install sqlite3
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release
```

Si CMake no encuentra SQLite3, instala SQLite3 y configura `CMAKE_PREFIX_PATH` o usa vcpkg.

## Uso

Ejecuta desde la carpeta raíz del proyecto para que `data/tpv.db` se cree en la ruta esperada; crea previamente la carpeta `data`
si no existe. El menú permite dar de alta artículos, cobrar importes libres, cobrar por artículo y gestionar caja. La impresora se
configura por su nombre de Windows en cada ejecución. La plantilla de ticket se encuentra en `../NexoBar/resources`;
puedes editar los textos y usar `{{BUSINESS_NAME}}`, `{{TICKET_ID}}`, `{{DATE}}`, `{{LINES}}`, `{{TOTAL}}` y `{{FOOTER}}`. Si la
plantilla no se encuentra, se usa una plantilla interna por defecto.

Para integrar con Raylib, instancia `tpv::TPV` y llama a sus métodos desde la lógica de la aplicación. Los importes son céntimos
enteros (`tpv::Cents`); `quantityMilli=1000` representa una unidad.

## Hardware

La impresora debe admitir ESC/POS y trabajos RAW. El pulso de cajón usa ESC/POS `ESC p 0 25 250`; comprueba el pin/cableado y el
pulso admitido por el modelo antes de conectarlo. No conectes el cajón directamente al USB del PC: normalmente va al RJ11/RJ12 de
la impresora.

## Mantenimiento: comprobación, backup y restauración

Se incluye el ejecutable `tpv_maintenance` para el paso de validación de almacenamiento. Usa la API online backup de SQLite y
comprueba `PRAGMA integrity_check` y `PRAGMA foreign_key_check`.

```powershell
# Comprobar la base activa
.\build\Release\tpv_maintenance.exe check .\data\tpv.db

# Crear una copia consistente y verificarla
New-Item -ItemType Directory -Force .\backups | Out-Null
.\build\Release\tpv_maintenance.exe backup .\data\tpv.db .\backups\tpv-test.db

# Restaurar a una ruta temporal (no sobrescribe la base activa)
.\build\Release\tpv_maintenance.exe restore .\backups\tpv-test.db .\data\restore-test.db
.\build\Release\tpv_maintenance.exe check .\data\restore-test.db

# Prueba automática de backup/restauración con una base temporal de prueba
.\build\Release\tpv_maintenance.exe self-test
ctest --test-dir build -C Release --output-on-failure
```

El ejecutable crea una restauración en un archivo distinto. Para sustituir la base activa, detén el TPV, conserva una copia de
seguridad de la base actual y sigue un procedimiento de recuperación controlado. Estas funciones verifican la integridad
estructural de SQLite; **no prueban la integridad fiscal ni implementan la conformidad AEAT**.

## Pruebas AEAT y aceptación de producción

- `../tests/AEAT_CONFORMANCE_TEST_PLAN.md`: casos que deben reproducir los ejemplos y validadores oficiales de la AEAT. Permanecen
  bloqueados hasta implementar el módulo fiscal.
- `../tests/PRODUCTION_ACCEPTANCE.md`: pruebas de copias, recuperación, exportación y hardware. Las pruebas de exportación fiscal
  y de impresora real siguen pendientes.

No marcar estos casos como aprobados sin guardar resultados reproducibles. La documentación oficial y los vectores de prueba deben
descargarse desde los enlaces de AEAT incluidos en el plan y versionarse con la fecha de consulta.

```


---

## `FISCAL_ES.md`

```markdown
# Situación fiscal del prototipo y ruta de trabajo

## Estado

Este proyecto es un prototipo operativo y **no se declara conforme** al RD 1007/2023 ni a la Orden HAC/1177/2024. No debe utilizarse para expedir facturas reales hasta implementar y verificar los requisitos aplicables y completar la declaración responsable del productor para la versión concreta.

Los tickets impresos actualmente son tickets operativos de prueba. No deben presentarse como facturas fiscales ni se convierten en registros fiscales por enviarlos posteriormente a una gestoría.

## Modalidad seleccionada: NO VERI*FACTU

NO VERI*FACTU no significa que no haya requisitos fiscales. En esta modalidad deben implementarse, según las especificaciones vigentes, los registros de facturación de alta y anulación, su encadenamiento y huella, firma XAdES Enveloped Signature de los registros de facturación y de evento, registro de eventos, QR fiscal, conservación, exportación y herramientas para comprobar integridad y firma. La emisión de la factura y la generación del registro correspondiente no pueden separarse como un reproceso posterior.

No basta con un hash SHA-256 aislado, una tabla SQLite, un PDF con QR o un archivo enviado a la gestoría.

## Plan de implementación y validación

1. Acordar con la gestoría los tipos de documentos, los tipos de IVA, numeración y obligaciones de facturación del bar.
2. Implementar pruebas automatizadas que reproduzcan literalmente los vectores, ejemplos, esquemas y validaciones publicados por la AEAT. El plan está en `tests/AEAT_CONFORMANCE_TEST_PLAN.md`.
3. Probar integridad, restauración, copias de seguridad, exportaciones, cortes de energía y errores de hardware conforme a `tests/PRODUCTION_ACCEPTANCE.md`. El ejecutable `tpv_maintenance` aporta comprobaciones estructurales de SQLite y pruebas de backup/restore; no acredita la integridad fiscal.
4. Implementar y verificar el módulo fiscal oficial completo, incluida firma XAdES y QR fiscal, antes de emitir facturas reales.
5. Validar el sistema con una persona especializada en facturación/fiscalidad y pruebas reales controladas en Windows.
6. Preparar la declaración responsable del productor para la versión concreta, con la identificación del sistema, versión, composición, funcionalidades y demás contenido exigible. No firmarla ni publicarla hasta poder sostener las afirmaciones de conformidad.
7. Mostrar la declaración responsable de forma visible y conservarla por versión; mantener control de cambios y un procedimiento de actualización.

La declaración responsable es responsabilidad del productor. Este proyecto no está certificado por Hacienda y no debe describirse como homologado.

## Fuentes oficiales

- AEAT — Sistemas informáticos de facturación y Veri*Factu: https://sede.agenciatributaria.gob.es/Sede/iva/sistemas-informaticos-facturacion-verifactu.html
- AEAT — Información técnica, esquemas, validaciones, huella, firma y QR: https://sede.agenciatributaria.gob.es/Sede/iva/sistemas-informaticos-facturacion-verifactu/informacion-tecnica.html
- AEAT — Algoritmo de huella: https://sede.agenciatributaria.gob.es/Sede/iva/sistemas-informaticos-facturacion-verifactu/informacion-tecnica/algoritmo-calculo-codificacion-huella-hash.html
- AEAT — Especificaciones de firma: https://sede.agenciatributaria.gob.es/Sede/iva/sistemas-informaticos-facturacion-verifactu/informacion-tecnica/especificaciones-tecnicas-firma-electronica-registros-evento.html
- BOE — RD 1007/2023: https://www.boe.es/eli/es/rd/2023/12/05/1007
- BOE — Orden HAC/1177/2024: https://www.boe.es/buscar/act.php?id=BOE-A-2024-22138

Consulta estas páginas de nuevo antes de desplegar: las especificaciones y los plazos pueden cambiar.
```

---

## `../NexoBar/CMakeLists.txt`

```text
cmake_minimum_required(VERSION 3.24)
project(TPVBackend VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SQLite3 REQUIRED)

add_executable(tpv_backend
    src/main.cpp
    src/Database.cpp
    src/TPV.cpp
    src/TicketPrinter.cpp
    src/TicketTemplate.cpp
)
target_include_directories(tpv_backend PRIVATE src)
target_link_libraries(tpv_backend PRIVATE SQLite::SQLite3)
if(WIN32)
    target_link_libraries(tpv_backend PRIVATE Winspool)
endif()
if(MSVC)
    target_compile_options(tpv_backend PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(tpv_backend PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(tpv_maintenance
    src/maintenance_main.cpp
    src/StorageMaintenance.cpp
)
target_include_directories(tpv_maintenance PRIVATE src)
target_link_libraries(tpv_maintenance PRIVATE SQLite::SQLite3)
if(MSVC)
    target_compile_options(tpv_maintenance PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(tpv_maintenance PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_test(NAME tpv_storage_backup_restore_selftest COMMAND tpv_maintenance self-test)
```

---

## `../NexoBar/models`

```cpp
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace tpv {

using Id = std::int64_t;
using Cents = std::int64_t;

enum class PaymentMethod { Cash, Card, Other };

inline std::string paymentMethodName(PaymentMethod method) {
    switch (method) {
        case PaymentMethod::Cash: return "EFECTIVO";
        case PaymentMethod::Card: return "TARJETA";
        case PaymentMethod::Other: return "OTRO";
    }
    return "OTRO";
}

struct Article {
    Id id{};
    std::string name;
    Cents priceCents{};
    bool active{true};
};

struct SaleLine {
    Id id{};
    std::optional<Id> articleId;
    std::string description;
    std::int64_t quantityMilli{1000};
    Cents unitPriceCents{};
    Cents totalCents{};
};

struct Sale {
    Id id{};
    std::string createdAt;
    std::string status;
    Cents totalCents{};
    std::vector<SaleLine> lines;
};

struct PaymentResult {
    bool ok{false};
    std::string error;
    Id saleId{};
    Cents totalCents{};
    Cents tenderedCents{};
    Cents changeCents{};
};

struct CashSession {
    Id id{};
    std::string openedAt;
    std::optional<std::string> closedAt;
    Cents openingCents{};
    std::optional<Cents> countedCents;
};

struct CashSummary {
    Id sessionId{};
    Cents openingCents{};
    Cents cashSalesCents{};
    Cents cashInCents{};
    Cents cashOutCents{};
    Cents expectedCents{};
};

inline Cents lineTotal(Cents unitPriceCents, std::int64_t quantityMilli) {
    const auto numerator = unitPriceCents * quantityMilli;
    return (numerator >= 0 ? numerator + 500 : numerator - 500) / 1000;
}

inline std::string formatMoney(Cents cents) {
    const bool negative = cents < 0;
    const auto absolute = negative ? -cents : cents;
    const auto euros = absolute / 100;
    const auto remainder = absolute % 100;
    std::string result = negative ? "-" : "";
    result += std::to_string(euros) + ",";
    if (remainder < 10) result += "0";
    result += std::to_string(remainder) + " EUR";
    return result;
}

} // namespace tpv
```

---

## `../old_`

```cpp
#pragma once

#include <sqlite3.h>
#include <cstdint>
#include <string>
#include <stdexcept>

namespace tpv {

class Database {
public:
    explicit Database(const std::string& path);
    ~Database();
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    sqlite3* handle() const noexcept { return db_; }
    void execute(const std::string& sql) const;
    void initializeSchema();

private:
    sqlite3* db_{};
};

class Statement {
public:
    Statement(sqlite3* db, const std::string& sql);
    ~Statement();
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    sqlite3_stmt* get() const noexcept { return stmt_; }
    void bindInt(int index, std::int64_t value);
    void bindText(int index, const std::string& value);
    void bindNull(int index);
    bool stepRow();
    void stepDone();
    std::int64_t int64(int column) const;
    std::string text(int column) const;
    bool isNull(int column) const;
    void reset();

private:
    sqlite3_stmt* stmt_{};
};

class Transaction {
public:
    explicit Transaction(const Database& db);
    ~Transaction();
    void commit();
private:
    const Database& db_;
    bool committed_{false};
};

} // namespace tpv
```

---

## `../old_`

```cpp
#include "Database.h"

namespace tpv {
namespace {
void check(int code, sqlite3* db, const std::string& context) {
    if (code != SQLITE_OK) throw std::runtime_error(context + ": " + sqlite3_errmsg(db));
}
}

Database::Database(const std::string& path) {
    const int rc = sqlite3_open_v2(path.c_str(), &db_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr);
    if (rc != SQLITE_OK) {
        const std::string error = db_ ? sqlite3_errmsg(db_) : "no se pudo abrir SQLite";
        if (db_) sqlite3_close(db_);
        db_ = nullptr;
        throw std::runtime_error("Error abriendo base de datos: " + error);
    }
    sqlite3_busy_timeout(db_, 5000);
    execute("PRAGMA foreign_keys=ON;");
    execute("PRAGMA journal_mode=WAL;");
    execute("PRAGMA synchronous=FULL;");
}

Database::~Database() { if (db_) sqlite3_close(db_); }

void Database::execute(const std::string& sql) const {
    char* error = nullptr;
    const int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &error);
    if (rc != SQLITE_OK) {
        const std::string message = error ? error : sqlite3_errmsg(db_);
        sqlite3_free(error);
        throw std::runtime_error("SQLite: " + message);
    }
}

void Database::initializeSchema() {
    execute(R"SQL(
CREATE TABLE IF NOT EXISTS articles(
 id INTEGER PRIMARY KEY AUTOINCREMENT,
 name TEXT NOT NULL,
 price_cents INTEGER NOT NULL CHECK(price_cents >= 0),
 active INTEGER NOT NULL DEFAULT 1 CHECK(active IN (0,1)),
 created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now'))
);
CREATE TABLE IF NOT EXISTS sales(
 id INTEGER PRIMARY KEY AUTOINCREMENT,
 created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')),
 status TEXT NOT NULL CHECK(status IN ('OPEN','PAID','VOID')),
 total_cents INTEGER NOT NULL DEFAULT 0 CHECK(total_cents >= 0),
 paid_cents INTEGER NOT NULL DEFAULT 0 CHECK(paid_cents >= 0),
 change_cents INTEGER NOT NULL DEFAULT 0 CHECK(change_cents >= 0)
);
CREATE TABLE IF NOT EXISTS sale_items(
 id INTEGER PRIMARY KEY AUTOINCREMENT,
 sale_id INTEGER NOT NULL REFERENCES sales(id),
 article_id INTEGER REFERENCES articles(id),
 description TEXT NOT NULL,
 quantity_milli INTEGER NOT NULL CHECK(quantity_milli > 0),
 unit_price_cents INTEGER NOT NULL CHECK(unit_price_cents >= 0),
 total_cents INTEGER NOT NULL CHECK(total_cents >= 0)
);
CREATE TABLE IF NOT EXISTS payments(
 id INTEGER PRIMARY KEY AUTOINCREMENT,
 sale_id INTEGER NOT NULL REFERENCES sales(id),
 method TEXT NOT NULL CHECK(method IN ('EFECTIVO','TARJETA','OTRO')),
 amount_cents INTEGER NOT NULL CHECK(amount_cents >= 0),
 tendered_cents INTEGER NOT NULL CHECK(tendered_cents >= 0),
 change_cents INTEGER NOT NULL CHECK(change_cents >= 0),
 created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now'))
);
CREATE TABLE IF NOT EXISTS cash_sessions(
 id INTEGER PRIMARY KEY AUTOINCREMENT,
 opened_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')),
 closed_at TEXT,
 opening_cents INTEGER NOT NULL CHECK(opening_cents >= 0),
 counted_cents INTEGER CHECK(counted_cents >= 0),
 expected_cents INTEGER
);
CREATE TABLE IF NOT EXISTS cash_movements(
 id INTEGER PRIMARY KEY AUTOINCREMENT,
 session_id INTEGER NOT NULL REFERENCES cash_sessions(id),
 direction TEXT NOT NULL CHECK(direction IN ('IN','OUT')),
 amount_cents INTEGER NOT NULL CHECK(amount_cents > 0),
 reason TEXT NOT NULL,
 created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now'))
);
CREATE INDEX IF NOT EXISTS idx_sale_items_sale ON sale_items(sale_id);
CREATE INDEX IF NOT EXISTS idx_payments_sale ON payments(sale_id);
CREATE INDEX IF NOT EXISTS idx_sales_created ON sales(created_at);
)SQL");
}

Statement::Statement(sqlite3* db, const std::string& sql) {
    check(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt_, nullptr), db, "Preparando consulta");
}
Statement::~Statement() { if (stmt_) sqlite3_finalize(stmt_); }
void Statement::bindInt(int i, std::int64_t v) { check(sqlite3_bind_int64(stmt_, i, v), sqlite3_db_handle(stmt_), "Enlazando entero"); }
void Statement::bindText(int i, const std::string& v) { check(sqlite3_bind_text(stmt_, i, v.c_str(), static_cast<int>(v.size()), SQLITE_TRANSIENT), sqlite3_db_handle(stmt_), "Enlazando texto"); }
void Statement::bindNull(int i) { check(sqlite3_bind_null(stmt_, i), sqlite3_db_handle(stmt_), "Enlazando NULL"); }
bool Statement::stepRow() { const int rc = sqlite3_step(stmt_); if (rc == SQLITE_ROW) return true; if (rc == SQLITE_DONE) return false; throw std::runtime_error(sqlite3_errmsg(sqlite3_db_handle(stmt_))); }
void Statement::stepDone() { const int rc = sqlite3_step(stmt_); if (rc != SQLITE_DONE) throw std::runtime_error(sqlite3_errmsg(sqlite3_db_handle(stmt_))); }
std::int64_t Statement::int64(int c) const { return sqlite3_column_int64(stmt_, c); }
std::string Statement::text(int c) const { const auto* v = sqlite3_column_text(stmt_, c); return v ? reinterpret_cast<const char*>(v) : ""; }
bool Statement::isNull(int c) const { return sqlite3_column_type(stmt_, c) == SQLITE_NULL; }
void Statement::reset() { sqlite3_reset(stmt_); sqlite3_clear_bindings(stmt_); }
Transaction::Transaction(const Database& db) : db_(db) { db_.execute("BEGIN IMMEDIATE;"); }
Transaction::~Transaction() { if (!committed_) { try { db_.execute("ROLLBACK;"); } catch (...) {} } }
void Transaction::commit() { db_.execute("COMMIT;"); committed_ = true; }
} // namespace tpv
```

---

## `../old_`

```cpp
#pragma once

#include "Database.h"
#include "Models.h"
#include "TicketPrinter.h"
#include <optional>
#include <string>
#include <vector>

namespace tpv {

class TPV {
public:
    explicit TPV(const std::string& databasePath);
    Database& database() noexcept { return db_; }

    Id createArticle(const std::string& name, Cents priceCents);
    void updateArticle(Id id, const std::string& name, Cents priceCents);
    void deactivateArticle(Id id);
    std::vector<Article> listArticles(bool activeOnly = true) const;

    Id createSale();
    void addArticle(Id saleId, Id articleId, std::int64_t quantityMilli = 1000);
    void addCustomItem(Id saleId, const std::string& description, Cents unitPriceCents,
                       std::int64_t quantityMilli = 1000);
    Sale getSale(Id saleId) const;
    PaymentResult pay(Id saleId, PaymentMethod method, Cents tenderedCents);
    PaymentResult quickCharge(Cents amountCents, PaymentMethod method, Cents tenderedCents,
                              const std::string& description = "Cobro manual");

    Id openCashRegister(Cents openingCents);
    void cashIn(Cents amountCents, const std::string& reason);
    void cashOut(Cents amountCents, const std::string& reason);
    CashSummary cashSummary() const;
    void closeCashRegister(Cents countedCents);

    TicketPrinter& printer() noexcept { return printer_; }
    bool printTicket(Id saleId, const std::string& businessName, std::string& error,
                     const std::string& footer = "Gracias por su visita");

private:
    Database db_;
    TicketPrinter printer_;
    Id activeCashSession() const;
    void recalculateSale(Id saleId);
    void requireOpenSale(Id saleId) const;
};

} // namespace tpv
```

---

## `../old_`

```cpp
#include "TPV.h"
#include "TicketTemplate.h"
#include <limits>
#include <stdexcept>

namespace tpv {
namespace {
void require(bool condition, const std::string& message) { if (!condition) throw std::invalid_argument(message); }
std::string methodToDb(PaymentMethod method) { return paymentMethodName(method); }
}

TPV::TPV(const std::string& path) : db_(path) { db_.initializeSchema(); }

Id TPV::createArticle(const std::string& name, Cents price) {
    require(!name.empty(), "El nombre del artículo no puede estar vacío.");
    require(price >= 0, "El precio no puede ser negativo.");
    Statement q(db_.handle(), "INSERT INTO articles(name,price_cents) VALUES(?,?)");
    q.bindText(1, name); q.bindInt(2, price); q.stepDone();
    return sqlite3_last_insert_rowid(db_.handle());
}
void TPV::updateArticle(Id id, const std::string& name, Cents price) {
    require(!name.empty() && price >= 0, "Nombre o precio no válido.");
    Statement q(db_.handle(), "UPDATE articles SET name=?,price_cents=? WHERE id=?");
    q.bindText(1, name); q.bindInt(2, price); q.bindInt(3, id); q.stepDone();
    if (sqlite3_changes(db_.handle()) == 0) throw std::runtime_error("No existe el artículo.");
}
void TPV::deactivateArticle(Id id) {
    Statement q(db_.handle(), "UPDATE articles SET active=0 WHERE id=?"); q.bindInt(1, id); q.stepDone();
    if (sqlite3_changes(db_.handle()) == 0) throw std::runtime_error("No existe el artículo.");
}
std::vector<Article> TPV::listArticles(bool activeOnly) const {
    Statement q(db_.handle(), activeOnly ? "SELECT id,name,price_cents,active FROM articles WHERE active=1 ORDER BY name" : "SELECT id,name,price_cents,active FROM articles ORDER BY name");
    std::vector<Article> result;
    while (q.stepRow()) result.push_back({q.int64(0), q.text(1), q.int64(2), q.int64(3) != 0});
    return result;
}
Id TPV::createSale() {
    Statement q(db_.handle(), "INSERT INTO sales(status,total_cents) VALUES('OPEN',0)"); q.stepDone();
    return sqlite3_last_insert_rowid(db_.handle());
}
void TPV::requireOpenSale(Id id) const {
    Statement q(db_.handle(), "SELECT status FROM sales WHERE id=?"); q.bindInt(1, id);
    if (!q.stepRow()) throw std::runtime_error("No existe la venta.");
    if (q.text(0) != "OPEN") throw std::runtime_error("La venta ya no está abierta.");
}
void TPV::addArticle(Id saleId, Id articleId, std::int64_t quantityMilli) {
    require(quantityMilli > 0, "La cantidad debe ser mayor que cero."); requireOpenSale(saleId);
    Statement a(db_.handle(), "SELECT name,price_cents FROM articles WHERE id=? AND active=1"); a.bindInt(1, articleId);
    if (!a.stepRow()) throw std::runtime_error("Artículo inexistente o inactivo.");
    const auto name = a.text(0); const auto price = a.int64(1);
    Statement q(db_.handle(), "INSERT INTO sale_items(sale_id,article_id,description,quantity_milli,unit_price_cents,total_cents) VALUES(?,?,?,?,?,?)");
    q.bindInt(1, saleId); q.bindInt(2, articleId); q.bindText(3, name); q.bindInt(4, quantityMilli); q.bindInt(5, price); q.bindInt(6, lineTotal(price, quantityMilli)); q.stepDone();
    recalculateSale(saleId);
}
void TPV::addCustomItem(Id saleId, const std::string& description, Cents price, std::int64_t quantityMilli) {
    require(!description.empty(), "El concepto no puede estar vacío."); require(price >= 0, "El importe no puede ser negativo."); require(quantityMilli > 0, "La cantidad debe ser mayor que cero."); requireOpenSale(saleId);
    Statement q(db_.handle(), "INSERT INTO sale_items(sale_id,article_id,description,quantity_milli,unit_price_cents,total_cents) VALUES(?,NULL,?,?,?,?)");
    q.bindInt(1, saleId); q.bindText(2, description); q.bindInt(3, quantityMilli); q.bindInt(4, price); q.bindInt(5, lineTotal(price, quantityMilli)); q.stepDone();
    recalculateSale(saleId);
}
void TPV::recalculateSale(Id id) {
    Statement q(db_.handle(), "UPDATE sales SET total_cents=(SELECT COALESCE(SUM(total_cents),0) FROM sale_items WHERE sale_id=?) WHERE id=? AND status='OPEN'");
    q.bindInt(1, id); q.bindInt(2, id); q.stepDone();
}
Sale TPV::getSale(Id id) const {
    Statement s(db_.handle(), "SELECT id,created_at,status,total_cents FROM sales WHERE id=?"); s.bindInt(1, id);
    if (!s.stepRow()) throw std::runtime_error("No existe la venta.");
    Sale sale{s.int64(0), s.text(1), s.text(2), s.int64(3), {}};
    Statement q(db_.handle(), "SELECT id,article_id,description,quantity_milli,unit_price_cents,total_cents FROM sale_items WHERE sale_id=? ORDER BY id"); q.bindInt(1, id);
    while (q.stepRow()) {
        SaleLine line; line.id=q.int64(0); if (!q.isNull(1)) line.articleId=q.int64(1); line.description=q.text(2); line.quantityMilli=q.int64(3); line.unitPriceCents=q.int64(4); line.totalCents=q.int64(5); sale.lines.push_back(std::move(line));
    }
    return sale;
}
PaymentResult TPV::pay(Id id, PaymentMethod method, Cents tendered) {
    require(tendered >= 0, "El importe entregado no puede ser negativo.");
    Transaction tx(db_); requireOpenSale(id);
    const auto sale = getSale(id);
    if (sale.lines.empty() || sale.totalCents <= 0) throw std::runtime_error("No se puede cobrar una venta vacía o de importe cero.");
    if (method != PaymentMethod::Cash && tendered < sale.totalCents) tendered = sale.totalCents;
    if (method == PaymentMethod::Cash && tendered < sale.totalCents) throw std::runtime_error("El efectivo entregado es inferior al total.");
    const Cents change = method == PaymentMethod::Cash ? tendered - sale.totalCents : 0;
    Statement p(db_.handle(), "INSERT INTO payments(sale_id,method,amount_cents,tendered_cents,change_cents) VALUES(?,?,?,?,?)");
    p.bindInt(1,id); p.bindText(2,methodToDb(method)); p.bindInt(3,sale.totalCents); p.bindInt(4,tendered); p.bindInt(5,change); p.stepDone();
    Statement u(db_.handle(), "UPDATE sales SET status='PAID',paid_cents=?,change_cents=? WHERE id=? AND status='OPEN'");
    u.bindInt(1,sale.totalCents); u.bindInt(2,change); u.bindInt(3,id); u.stepDone();
    tx.commit();
    return {true,{},id,sale.totalCents,tendered,change};
}
PaymentResult TPV::quickCharge(Cents amount, PaymentMethod method, Cents tendered, const std::string& description) {
    require(amount > 0, "El importe debe ser mayor que cero.");
    const Id id = createSale();
    try { addCustomItem(id, description, amount); return pay(id, method, tendered); }
    catch (...) { throw; }
}
Id TPV::activeCashSession() const {
    Statement q(db_.handle(), "SELECT id FROM cash_sessions WHERE closed_at IS NULL ORDER BY id DESC LIMIT 1");
    if (!q.stepRow()) throw std::runtime_error("No hay una caja abierta.");
    return q.int64(0);
}
Id TPV::openCashRegister(Cents opening) {
    require(opening >= 0, "El fondo inicial no puede ser negativo.");
    Statement active(db_.handle(), "SELECT id FROM cash_sessions WHERE closed_at IS NULL LIMIT 1");
    if (active.stepRow()) throw std::runtime_error("Ya hay una caja abierta.");
    Statement q(db_.handle(), "INSERT INTO cash_sessions(opening_cents) VALUES(?)"); q.bindInt(1,opening); q.stepDone();
    return sqlite3_last_insert_rowid(db_.handle());
}
void TPV::cashIn(Cents amount, const std::string& reason) {
    require(amount > 0 && !reason.empty(), "Importe y motivo son obligatorios."); const auto id=activeCashSession();
    Statement q(db_.handle(), "INSERT INTO cash_movements(session_id,direction,amount_cents,reason) VALUES(?,'IN',?,?)"); q.bindInt(1,id); q.bindInt(2,amount); q.bindText(3,reason); q.stepDone();
}
void TPV::cashOut(Cents amount, const std::string& reason) {
    require(amount > 0 && !reason.empty(), "Importe y motivo son obligatorios."); const auto id=activeCashSession();
    Statement q(db_.handle(), "INSERT INTO cash_movements(session_id,direction,amount_cents,reason) VALUES(?,'OUT',?,?)"); q.bindInt(1,id); q.bindInt(2,amount); q.bindText(3,reason); q.stepDone();
}
CashSummary TPV::cashSummary() const {
    const auto id=activeCashSession();
    Statement s(db_.handle(), "SELECT opening_cents FROM cash_sessions WHERE id=?"); s.bindInt(1,id); if(!s.stepRow()) throw std::runtime_error("No existe la caja.");
    const Cents opening=s.int64(0);
    Statement sales(db_.handle(), "SELECT COALESCE(SUM(p.amount_cents),0) FROM payments p JOIN sales s ON s.id=p.sale_id WHERE s.status='PAID' AND p.method='EFECTIVO' AND p.created_at >= (SELECT opened_at FROM cash_sessions WHERE id=?)"); sales.bindInt(1,id); sales.stepRow(); const Cents cashSales=sales.int64(0);
    Statement moves(db_.handle(), "SELECT COALESCE(SUM(CASE WHEN direction='IN' THEN amount_cents ELSE 0 END),0),COALESCE(SUM(CASE WHEN direction='OUT' THEN amount_cents ELSE 0 END),0) FROM cash_movements WHERE session_id=?"); moves.bindInt(1,id); moves.stepRow();
    const Cents in=moves.int64(0), out=moves.int64(1);
    return {id,opening,cashSales,in,out,opening+cashSales+in-out};
}
void TPV::closeCashRegister(Cents counted) {
    require(counted >= 0, "El efectivo contado no puede ser negativo."); const auto summary=cashSummary();
    Statement q(db_.handle(), "UPDATE cash_sessions SET closed_at=strftime('%Y-%m-%dT%H:%M:%fZ','now'),counted_cents=?,expected_cents=? WHERE id=? AND closed_at IS NULL");
    q.bindInt(1,counted); q.bindInt(2,summary.expectedCents); q.bindInt(3,summary.sessionId); q.stepDone();
}
bool TPV::printTicket(Id id, const std::string& businessName, std::string& error, const std::string& footer) {
    const auto sale=getSale(id);
    std::string content;
    try { content = TicketTemplate::renderFile("resources/tickets/ticket.tpl", sale, businessName, footer); }
    catch (const std::runtime_error&) { content = TicketTemplate::renderDefault(sale, businessName, footer); }
    content += "\n\n\n\x1D" "V" "\x00";
    return printer_.printRaw(std::string("\x1B" "@") + content, error);
}
} // namespace tpv
```

---

## `../NexoBar/printer`

```cpp
#pragma once

#include "Models.h"
#include <string>

namespace tpv {

class TicketPrinter {
public:
    explicit TicketPrinter(std::string printerName = {});
    void setPrinterName(std::string printerName);
    const std::string& printerName() const noexcept { return printerName_; }
    bool printRaw(const std::string& bytes, std::string& error) const;
    bool printText(const std::string& text, std::string& error) const;
    bool openDrawer(std::string& error) const;
    static std::string makeReceipt(const Sale& sale, const std::string& businessName,
                                   const std::string& footer = "Gracias por su visita");
private:
    std::string printerName_;
};

} // namespace tpv
```

---

## `../NexoBar/printer`

```cpp
#include "TicketPrinter.h"
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <winspool.h>
#endif

namespace tpv {
TicketPrinter::TicketPrinter(std::string name) : printerName_(std::move(name)) {}
void TicketPrinter::setPrinterName(std::string name) { printerName_ = std::move(name); }

bool TicketPrinter::printRaw(const std::string& bytes, std::string& error) const {
#ifdef _WIN32
    if (printerName_.empty()) { error = "Configura el nombre exacto de la impresora de Windows."; return false; }
    HANDLE printer = nullptr;
    if (!OpenPrinterA(const_cast<char*>(printerName_.c_str()), &printer, nullptr)) {
        error = "OpenPrinter ha fallado: " + std::to_string(GetLastError()); return false;
    }
    DOC_INFO_1A doc{};
    doc.pDocName = const_cast<char*>("TPV ticket ESC/POS");
    doc.pOutputFile = nullptr;
    doc.pDatatype = const_cast<char*>("RAW");
    const DWORD job = StartDocPrinterA(printer, 1, reinterpret_cast<LPBYTE>(&doc));
    if (!job) { error = "StartDocPrinter ha fallado: " + std::to_string(GetLastError()); ClosePrinter(printer); return false; }
    bool ok = StartPagePrinter(printer) != 0;
    DWORD written = 0;
    if (ok && !bytes.empty()) ok = WritePrinter(printer, const_cast<char*>(bytes.data()), static_cast<DWORD>(bytes.size()), &written) != 0 && written == bytes.size();
    if (ok) ok = EndPagePrinter(printer) != 0;
    EndDocPrinter(printer);
    if (!ok) error = "No se pudo enviar el trabajo RAW: " + std::to_string(GetLastError());
    ClosePrinter(printer);
    return ok;
#else
    (void)bytes; error = "La impresión ESC/POS RAW está implementada para Windows."; return false;
#endif
}

bool TicketPrinter::printText(const std::string& text, std::string& error) const {
    std::string data = "\x1B@";
    data += text;
    data += "\n\n\n\x1DVA\x00";
    return printRaw(data, error);
}

bool TicketPrinter::openDrawer(std::string& error) const {
    std::string pulse;
    pulse.push_back('\x1B'); pulse.push_back('\x70'); pulse.push_back('\x00'); pulse.push_back('\x19'); pulse.push_back('\xFA');
    return printRaw(pulse, error);
}

std::string TicketPrinter::makeReceipt(const Sale& sale, const std::string& businessName, const std::string& footer) {
    std::ostringstream out;
    out << '\x1B' << 'a' << '\x01' << businessName << "\n";
    out << "------------------------------------------\n";
    out << "Ticket: " << sale.id << "\nFecha: " << sale.createdAt << "\n";
    out << "------------------------------------------\n";
    out << "CONCEPTO                    IMPORTE\n";
    for (const auto& line : sale.lines) {
        out << line.description << "\n";
        out << "  " << line.quantityMilli / 1000;
        if (line.quantityMilli % 1000 != 0) out << "." << std::setw(3) << std::setfill('0') << line.quantityMilli % 1000 << std::setfill(' ');
        out << " x " << formatMoney(line.unitPriceCents) << "  " << formatMoney(line.totalCents) << "\n";
    }
    out << "------------------------------------------\n";
    out << "TOTAL: " << formatMoney(sale.totalCents) << "\n\n";
    out << '\x1B' << 'a' << '\x01' << footer << "\n" << '\x1B' << 'a' << '\x00';
    return out.str();
}
} // namespace tpv
```

---

## `../NexoBar/template`

```cpp
#pragma once
#include "Models.h"
#include <string>

namespace tpv {

class TicketTemplate {
public:
    static std::string renderFile(const std::string& path, const Sale& sale,
                                  const std::string& businessName,
                                  const std::string& footer = "Gracias por su visita");
    static std::string renderDefault(const Sale& sale, const std::string& businessName,
                                    const std::string& footer = "Gracias por su visita");
};

} // namespace tpv
```

---

## `../NexoBar/template`

```cpp
#include "TicketTemplate.h"
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace tpv {
namespace {
void replaceAll(std::string& text, const std::string& token, const std::string& value) {
    std::size_t pos = 0;
    while ((pos = text.find(token, pos)) != std::string::npos) {
        text.replace(pos, token.size(), value);
        pos += value.size();
    }
}
std::string linesFor(const Sale& sale) {
    std::ostringstream out;
    for (const auto& line : sale.lines) {
        out << line.description << "\n";
        out << "  " << line.quantityMilli / 1000;
        if (line.quantityMilli % 1000 != 0) out << "." << (line.quantityMilli % 1000 < 100 ? "0" : "") << (line.quantityMilli % 1000 < 10 ? "0" : "") << line.quantityMilli % 1000;
        out << " x " << formatMoney(line.unitPriceCents) << " = " << formatMoney(line.totalCents) << "\n";
    }
    return out.str();
}
std::string apply(std::string text, const Sale& sale, const std::string& businessName, const std::string& footer) {
    replaceAll(text, "{{BUSINESS_NAME}}", businessName);
    replaceAll(text, "{{TICKET_ID}}", std::to_string(sale.id));
    replaceAll(text, "{{DATE}}", sale.createdAt);
    replaceAll(text, "{{LINES}}", linesFor(sale));
    replaceAll(text, "{{TOTAL}}", formatMoney(sale.totalCents));
    replaceAll(text, "{{FOOTER}}", footer);
    return text;
}
}
std::string TicketTemplate::renderFile(const std::string& path, const Sale& sale, const std::string& businessName, const std::string& footer) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("No se pudo abrir la plantilla de ticket: " + path);
    std::ostringstream content; content << file.rdbuf();
    return apply(content.str(), sale, businessName, footer);
}
std::string TicketTemplate::renderDefault(const Sale& sale, const std::string& businessName, const std::string& footer) {
    const std::string text = "\x1B" "a" "\x01" "{{BUSINESS_NAME}}\n"
        "------------------------------------------\n"
        "Ticket: {{TICKET_ID}}\nFecha: {{DATE}}\n"
        "------------------------------------------\n"
        "{{LINES}}"
        "------------------------------------------\n"
        "TOTAL: {{TOTAL}}\n\n"
        "\x1B" "a" "\x01" "{{FOOTER}}\n"
        "\x1B" "a" "\x00";
    return apply(text, sale, businessName, footer);
}
} // namespace tpv
```

---

## `../old_`

```cpp
#pragma once

#include <string>
#include <vector>

namespace tpv {

struct IntegrityReport {
    bool ok{false};
    std::vector<std::string> messages;
};

class StorageMaintenance {
public:
    // Comprueba PRAGMA integrity_check y PRAGMA foreign_key_check.
    // No modifica la base de datos.
    static IntegrityReport verify(const std::string& databasePath);

    // Crea una copia consistente usando la API de backup online de SQLite.
    // Puede ejecutarse con la base abierta por la aplicación.
    static void backup(const std::string& sourcePath, const std::string& backupPath);

    // Restaura la copia en una ruta DESTINO distinta. Por seguridad no sobrescribe
    // el archivo de origen; el llamador debe cerrar la aplicación y decidir cómo
    // sustituir la base de datos activa después de verificar el destino.
    static void restore(const std::string& backupPath, const std::string& destinationPath);
};

} // namespace tpv
```

---

## `../old_`

```cpp
#include "StorageMaintenance.h"

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
```

---

## `../old_`

```cpp
#include "StorageMaintenance.h"

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
```

---

## `../old_`

```cpp
#include "TPV.h"
#include <iostream>
#include <filesystem>
#include <limits>
#include <string>

using namespace tpv;

namespace {
Cents readMoney(const std::string& prompt) {
    while (true) {
        std::cout << prompt << " (euros, ej. 12.50): ";
        std::string value; std::cin >> value;
        for (char& c : value) if (c == ',') c = '.';
        try {
            const auto dot = value.find('.');
            const std::string eurosText = dot == std::string::npos ? value : value.substr(0,dot);
            const std::string centsText = dot == std::string::npos ? "" : value.substr(dot+1);
            if (eurosText.empty() || centsText.size() > 2) throw std::invalid_argument("importe");
            long long euros = std::stoll(eurosText);
            long long cents = centsText.empty() ? 0 : std::stoll(centsText);
            if (centsText.size() == 1) cents *= 10;
            if (euros < 0 || cents < 0) throw std::invalid_argument("negativo");
            return euros*100+cents;
        } catch (...) { std::cout << "Importe no válido.\n"; }
    }
}
}

int main() {
    try {
        std::filesystem::create_directories("data");
        TPV tpv("data/tpv.db");
        std::cout << "TPV backend de pruebas (no es un sistema fiscal certificado)\n";
        bool running = true;
        while (running) {
            std::cout << "\n1) Listar artículos  2) Crear artículo  3) Cobro manual  4) Venta por artículo\n"
                         "5) Abrir caja  6) Resumen caja  7) Entrada caja  8) Salida caja\n"
                         "9) Cerrar caja  10) Configurar impresora  11) Imprimir último ticket  0) Salir\n> ";
            int choice{}; if (!(std::cin >> choice)) break;
            try {
                if (choice == 0) running = false;
                else if (choice == 1) {
                    for (const auto& a : tpv.listArticles()) std::cout << a.id << ") " << a.name << " - " << formatMoney(a.priceCents) << '\n';
                } else if (choice == 2) {
                    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); std::string name;
                    std::cout << "Nombre: "; std::getline(std::cin,name); const auto price=readMoney("Precio");
                    std::cout << "Artículo creado con ID " << tpv.createArticle(name,price) << '\n';
                } else if (choice == 3) {
                    const auto amount=readMoney("Importe a cobrar"); std::cout << "Concepto: "; std::cin.ignore(std::numeric_limits<std::streamsize>::max(),'\n'); std::string desc; std::getline(std::cin,desc); if(desc.empty()) desc="Cobro manual";
                    std::cout << "Método (1 efectivo, 2 tarjeta, 3 otro): "; int m; std::cin>>m; const auto method=m==2?PaymentMethod::Card:(m==3?PaymentMethod::Other:PaymentMethod::Cash);
                    const auto tendered=method==PaymentMethod::Cash?readMoney("Efectivo entregado"):amount;
                    const auto r=tpv.quickCharge(amount,method,tendered,desc); std::cout << "Venta "<<r.saleId<<" cobrada. Total "<<formatMoney(r.totalCents)<<", cambio "<<formatMoney(r.changeCents)<<'\n';
                } else if (choice == 4) {
                    const auto articles=tpv.listArticles(); for(const auto& a:articles) std::cout<<a.id<<") "<<a.name<<" - "<<formatMoney(a.priceCents)<<'\n';
                    std::cout<<"ID artículo: "; Id articleId; std::cin>>articleId; auto sale=tpv.createSale(); tpv.addArticle(sale,articleId);
                    const auto total=tpv.getSale(sale).totalCents; std::cout<<"Total: "<<formatMoney(total)<<"\nMétodo (1 efectivo, 2 tarjeta, 3 otro): "; int m; std::cin>>m; const auto method=m==2?PaymentMethod::Card:(m==3?PaymentMethod::Other:PaymentMethod::Cash);
                    const auto tendered=method==PaymentMethod::Cash?readMoney("Efectivo entregado"):total; const auto r=tpv.pay(sale,method,tendered); std::cout<<"Venta "<<r.saleId<<" cobrada. Cambio "<<formatMoney(r.changeCents)<<'\n';
                } else if (choice == 5) {
                    const auto opening=readMoney("Fondo inicial"); std::cout<<"Caja abierta, sesión "<<tpv.openCashRegister(opening)<<'\n';
                } else if (choice == 6) {
                    const auto s=tpv.cashSummary(); std::cout<<"Fondo: "<<formatMoney(s.openingCents)<<"\nVentas efectivo: "<<formatMoney(s.cashSalesCents)<<"\nEntradas: "<<formatMoney(s.cashInCents)<<"\nSalidas: "<<formatMoney(s.cashOutCents)<<"\nEsperado: "<<formatMoney(s.expectedCents)<<'\n';
                } else if (choice == 7 || choice == 8) {
                    const auto amount=readMoney("Importe"); std::cin.ignore(std::numeric_limits<std::streamsize>::max(),'\n'); std::string reason; std::cout<<"Motivo: "; std::getline(std::cin,reason); if(choice==7)tpv.cashIn(amount,reason);else tpv.cashOut(amount,reason); std::cout<<"Movimiento registrado.\n";
                } else if (choice == 9) {
                    const auto counted=readMoney("Efectivo contado"); tpv.closeCashRegister(counted); std::cout<<"Caja cerrada.\n";
                } else if (choice == 10) {
                    std::cin.ignore(std::numeric_limits<std::streamsize>::max(),'\n'); std::string name; std::cout<<"Nombre de impresora en Windows: "; std::getline(std::cin,name); tpv.printer().setPrinterName(name); std::cout<<"Impresora configurada para esta ejecución.\n";
                } else if (choice == 11) {
                    std::cout<<"ID de venta: "; Id id; std::cin>>id; std::string error; if(!tpv.printTicket(id,"Mi bar",error))std::cout<<"Error de impresión: "<<error<<'\n'; else std::cout<<"Trabajo enviado a la impresora.\n";
                } else std::cout<<"Opción desconocida.\n";
            } catch(const std::exception& e) { std::cout<<"Error: "<<e.what()<<'\n'; }
        }
    } catch(const std::exception& e) { std::cerr<<"Error fatal: "<<e.what()<<'\n'; return 1; }
    return 0;
}
```

---

## `../NexoBar/resources`

```text
{{BUSINESS_NAME}}
------------------------------------------
Ticket: {{TICKET_ID}}
Fecha: {{DATE}}
------------------------------------------
{{LINES}}------------------------------------------
TOTAL: {{TOTAL}}

{{FOOTER}}
```

---

## `../tests/AEAT_CONFORMANCE_TEST_PLAN.md`

```markdown
# Plan de pruebas de conformidad técnica AEAT — NO VERI*FACTU

**Estado: BLOQUEADO / NO IMPLEMENTADO.** El backend actual no contiene todavía el módulo fiscal, el generador XML fiscal, el
cálculo oficial de huellas, la firma XAdES, el registro de eventos fiscal ni el QR de factura. Por tanto, estas pruebas no se
pueden marcar como superadas ejecutando el prototipo actual.

Este documento operacionaliza el paso 2: reproducir las especificaciones y ejemplos oficiales publicados por la AEAT. No reemplaza
los documentos oficiales ni crea vectores esperados propios.

## Fuentes de referencia

- Portal técnico
  AEAT: https://sede.agenciatributaria.gob.es/Sede/iva/sistemas-informaticos-facturacion-verifactu/informacion-tecnica.html
- Diseños de registro, esquemas, validaciones y errores: disponibles desde el portal técnico anterior.
- Algoritmo de
  huella: https://sede.agenciatributaria.gob.es/Sede/iva/sistemas-informaticos-facturacion-verifactu/informacion-tecnica/algoritmo-calculo-codificacion-huella-hash.html
- Firma electrónica de
  registros: https://sede.agenciatributaria.gob.es/Sede/iva/sistemas-informaticos-facturacion-verifactu/informacion-tecnica/especificaciones-tecnicas-firma-electronica-registros-evento.html
- Orden HAC/1177/2024: https://www.boe.es/buscar/act.php?id=BOE-A-2024-22138

Antes de ejecutar las pruebas, descarga los ejemplos oficiales vigentes y conserva una copia con fecha/versión en
`tests/fixtures/aeat/`. No edites manualmente las salidas esperadas de la AEAT.

## Casos obligatorios al implementar `FiscalService`

| ID | Prueba | Resultado esperado | Estado actual |
|---|---|---|---|
| AEAT-01 | Serialización de un registro de alta | XML UTF-8 conforme al XSD oficial; valores/campos y orden exactos | BLOQUEADO: falta FiscalService |
| AEAT-02 | Registro de anulación | XML conforme al diseño oficial y referencia al registro correspondiente | BLOQUEADO |
| AEAT-03 | Huella del alta con vector oficial | Coincidencia exacta con el valor oficial | BLOQUEADO |
| AEAT-04 | Huella de anulación con vector oficial | Coincidencia exacta con el valor oficial | BLOQUEADO |
| AEAT-05 | Encadenamiento de registros | El hash previo coincide y la secuencia es comprobable hacia delante/atrás | BLOQUEADO |
| AEAT-06 | Firma XAdES Enveloped Signature | Validación criptográfica positiva con certificado de prueba admitido | BLOQUEADO |
| AEAT-07 | Firma alterada | La comprobación debe fallar tras alterar un byte firmado | BLOQUEADO |
| AEAT-08 | Registro de evento y su huella/firma | Coincide con el diseño y especificación oficial | BLOQUEADO |
| AEAT-09 | QR fiscal | URL y datos exactos según la especificación técnica vigente | BLOQUEADO |
| AEAT-10 | Rechazo de XML inválido | El validador lo rechaza con error identificable | BLOQUEADO |
| AEAT-11 | Prueba de fechas, zona horaria y cambios de día | Formato y secuencia temporal conforme a la especificación | BLOQUEADO |
| AEAT-12 | Exportación de registros de un periodo | Todos los registros requeridos, legibles, verificables y sin omisiones | BLOQUEADO |

## Cómo registrar resultados

Para cada prueba guarda: identificador, versión del software, versión/fecha de la especificación AEAT, fixture de entrada, salida
generada, salida esperada, herramienta usada para comparar, resultado y logs. Los certificados y claves de prueba no deben ser los
certificados ni las claves de producción.

No se considerará aprobada una prueba por comparar solo el hash de un JSON inventado, ni por obtener una firma válida sobre un
formato distinto al exigido. Debe probarse el XML/canonización/campos exactos definidos por AEAT.
```

---

## `../tests/PRODUCTION_ACCEPTANCE.md`

```markdown
# Pruebas de integridad, recuperación, exportación y hardware

**Estado: pruebas de mantenimiento SQLite implementadas; pruebas fiscales y de hardware requieren integración en Windows y siguen
pendientes.**

## A. Integridad y recuperación de base de datos

1. Ejecutar `tpv_maintenance check data/tpv.db` y guardar la salida.
2. Ejecutar `tpv_maintenance backup data/tpv.db backups/tpv-YYYYMMDD-HHMM.db`.
3. Verificar la copia con `tpv_maintenance check backups/tpv-YYYYMMDD-HHMM.db`.
4. Restaurar siempre primero a una ruta temporal: `tpv_maintenance restore backups/tpv-YYYYMMDD-HHMM.db data/restore-test.db`.
5. Ejecutar `tpv_maintenance check data/restore-test.db`.
6. Comparar recuentos de ventas, líneas, pagos, artículos y sesiones de caja entre original y restaurada. La comprobación SQLite
   no demuestra por sí sola que los datos fiscales sean auténticos ni completos.
7. Simular falta de espacio, permisos denegados, archivo inexistente, destino ocupado y fallo de escritura; confirmar que el
   programa informa del error y no declara una copia correcta.
8. Ensayar recuperación en un equipo de prueba y documentar tiempo de recuperación y pérdida de datos esperada.

### Precauciones

- Mantener copias fuera del disco del TPV y cifrarlas si contienen datos personales.
- Restringir el acceso a las copias.
- La herramienta de restauración crea un archivo de destino diferente; no sustituye automáticamente la base activa.
- Para sustituir la base activa, cerrar primero el TPV, guardar una copia de la base actual y aplicar un procedimiento controlado.
  No borrar ni editar registros fiscales a mano.
- Una copia SQLite íntegra no sustituye a la firma, el encadenamiento ni la exportación exigidos al SIF.

## B. Exportación

Actualmente no existe exportador fiscal en el prototipo. Antes de producción, cuando se implemente, comprobar:

- Exportación por periodo y por tipo de registro, incluyendo alta, anulación y eventos que correspondan.
- UTF-8 y estructura oficial; no convertir los registros a CSV como único archivo de conservación.
- Recuento de registros exportados frente a la consulta de origen.
- Revisión de que cada registro exportado conserva huella, firma, encadenamiento y referencias.
- Exportación repetida produce los mismos registros lógicos, sin duplicar ni omitir datos.
- Los errores de exportación se registran y no se reporta éxito parcial como éxito completo.

## C. Impresora y cajón — prueba física Windows 11

- Probar con la impresora real y un rollo de prueba; validar UTF-8/codificación, corte, ancho, saltos, importes y alineación.
- Desconectar la impresora, provocar cola atascada y falta de papel si el dispositivo lo permite; el backend debe reportar fallo y
  no confundir “venta guardada” con “ticket impreso”.
- Verificar apertura del cajón por RJ11/RJ12 conectado a la impresora. No se debe disparar el cajón automáticamente antes de
  confirmar la operación de cobro.
- Si el ticket falla después del cobro, permitir reimpresión claramente identificada sin crear una segunda venta ni un nuevo
  registro fiscal.
- La prueba de impresión no es una prueba de conformidad fiscal.

## D. Integridad de registros fiscales (pendiente del módulo fiscal)

- Alterar una copia de prueba de un registro y confirmar que falla la verificación de huella/firma/cadena.
- Restaurar una copia y verificar cadena y firmas completas.
- Verificar registro de evento para arranque/parada, comprobaciones, anomalías, restauraciones y exportaciones según el modo NO
  VERI*FACTU.
- Comprobar que correcciones/anulaciones añaden los registros exigidos y nunca borran silenciosamente el original.
```
