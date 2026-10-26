# TPV backend C++20 (prototipo operativo)

Consulta [`docs/DEVELOPER_GUIDE.md`](docs/DEVELOPER_GUIDE.md) para la arquitectura, persistencia, UI, permisos, ventas,
impresión y guía de ampliación del proyecto.

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
- Boost con los componentes `date_time` y `serialization`
- nlohmann-json (paquete CMake `nlohmann_json`, disponible mediante vcpkg)
- Windows 11 para impresión RAW con Winspool

Ejemplo con vcpkg:

```powershell
vcpkg install sqlite3 boost-date-time boost-serialization nlohmann-json
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release
```

Si CMake no encuentra SQLite3 o Boost, instala las dependencias y configura `CMAKE_PREFIX_PATH` o usa vcpkg.

## Uso

Ejecuta desde la carpeta raíz del proyecto para que `data/tpv.db` se cree en la ruta esperada; crea previamente la carpeta `data`
si no existe. El menú permite dar de alta artículos, cobrar importes libres, cobrar por artículo y gestionar caja. La impresora se
configura por su nombre de Windows en cada ejecución. La plantilla de ticket se encuentra en `NexoBar/resources`; puedes editar
los textos y usar `{{BUSINESS_NAME}}`, `{{TICKET_ID}}`, `{{DATE}}`, `{{LINES}}`, `{{TOTAL}}` y `{{FOOTER}}`. Si la plantilla no se
encuentra, se usa una plantilla interna por defecto.

Para integrar con Raylib, instancia `tpv::TPV` y llama a sus métodos desde la lógica de la aplicación. Los importes son céntimos
enteros (`tpv::Cents`); `quantityMilli=1000` representa una unidad.

## Database y dominio de hosteler?a

La capa SQLite/ORM y el dominio de hosteler?a viven juntos en `Platform/database`. El dominio antes llamado POS ahora se llama
`hospitality`, en `Platform/database/hospitality`, namespace `nexobar`. Las entidades persistidas son clases, con una entidad por
archivo en `Platform/database/models`; `Models.h` re?ne los modelos cuando hacen falta todos.

```cpp
#include "database/hospitality/HospitalityDatabase.h"
#include "i18n/I18n.h"

using namespace nexobar;
platform::I18n i18n("NexoBar/resources/languages");
auto database = openHospitalityDatabase("data/nexobar.db", i18n);

Product product;
product.name = "Caf?";
product.price = 150; // c?ntimos
const ProductId id = database->insert<Product>(product);
const auto stored = database->select<Product>(id);
```

Para mapeo, consultas, `NamedType`, `UNION`, joins, transacciones, codecs propios y migraciones,
consulta [la gu?a de la base de datos](Platform/database/README.md).
Los catálogos editables están en `NexoBar/resources/languages` (por ejemplo `es-ES.json` y `en-US.json`). NexoBar conserva un
`std::unique_ptr<platform::I18n>` y detecta el idioma de interfaz preferido de Windows; `nexo->i18n()->get("roles.manager.name")`
permite consultar traducciones desde la interfaz. Al crear una base, los datos iniciales visibles se guardan con ese idioma;
los códigos internos como `CASH` y `CARD` permanecen estables en inglés.

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

- `tests/AEAT_CONFORMANCE_TEST_PLAN.md`: casos que deben reproducir los ejemplos y validadores oficiales de la AEAT. Permanecen
  bloqueados hasta implementar el módulo fiscal.
- `tests/PRODUCTION_ACCEPTANCE.md`: pruebas de copias, recuperación, exportación y hardware. Las pruebas de exportación fiscal y
  de impresora real siguen pendientes.

No marcar estos casos como aprobados sin guardar resultados reproducibles. La documentación oficial y los vectores de prueba deben
descargarse desde los enlaces de AEAT incluidos en el plan y versionarse con la fecha de consulta.
