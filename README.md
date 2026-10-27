# NexoBar

TPV táctil C++20 para barras, con ventas, reportes, usuarios/roles/permisos, edición de tickets e impresión térmica USB directa.

La descripción completa del estado actual, arquitectura y funcionamiento está en
[`docs/CAMBIOS_Y_FUNCIONAMIENTO_ACTUAL.md`](docs/CAMBIOS_Y_FUNCIONAMIENTO_ACTUAL.md). La guía para ampliar el código está en
[`docs/DEVELOPER_GUIDE.md`](docs/DEVELOPER_GUIDE.md).

## Compilación

Requisitos: CMake 3.24+, Ninja, compilador C++20 y `VCPKG_ROOT`. El manifiesto instala SQLite, Boost, FTXUI, raylib, libusb,
nlohmann-json y GTest.

```bash
cmake --preset ci-windows
cmake --build --preset ci-windows
ctest --preset ci-windows
```

En Linux se usa `ci-linux`. Para desarrollo local en Windows está disponible el preset `dev`.

## Ejecutables

- `NexoBar`: interfaz táctil. Mantiene cabecera y menú durante la sesión y cambia únicamente el contenido central.
- `Management`: gestión de licencias, alta de negocio y creación de un negocio simulado con datos funcionales.

La base SQLite se crea como `nexobar.db`. La inicialización añade de forma idempotente el esquema hostelero, roles, permisos,
usuario inicial, impuestos y categorías.

## Impresión

NexoBar solo enumera dispositivos USB mediante libusb; no utiliza Winspool, CUPS ni colas del sistema. Reconoce impresoras de
clase USB y perfiles de fabricantes térmicos habituales, infiere el ancho de papel y selecciona ESC/POS, StarPRNT, ZPL o Brother
raster según el dispositivo. En Windows la interfaz necesita un controlador WinUSB/libusb; en Linux necesita permisos de acceso
al dispositivo. El cajón se conecta a la impresora, no directamente al ordenador.

## Idiomas y recursos

Los textos están en `NexoBar/resources/languages/es-ES.json` y `en-US.json`, y CMake los embebe en NexoBar y Management. Español
es el idioma de fallback. Texturas, fuentes, sonidos y música tienen cargadores y cachés separados detrás de `AssetManager`.

## Aviso fiscal

Este repositorio es un prototipo operativo y no se declara conforme al RD 1007/2023, la Orden HAC/1177/2024 ni Veri*Factu. No
debe utilizarse para facturación real hasta implementar y validar todos los requisitos fiscales aplicables.
