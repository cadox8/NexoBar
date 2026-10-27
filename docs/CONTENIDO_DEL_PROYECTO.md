# Contenido actual del proyecto

Este documento sustituye al antiguo volcado de fuentes, que describía un prototipo ya retirado.
La referencia funcional y técnica mantenida del estado actual se encuentra en
[`CAMBIOS_Y_FUNCIONAMIENTO_ACTUAL.md`](CAMBIOS_Y_FUNCIONAMIENTO_ACTUAL.md).

## Módulos

- `Platform`: persistencia SQLite, esquema hostelero, modelos, internacionalización y utilidades compartidas.
- `NexoBar`: aplicación táctil, screens/componentes, ventas, reportes, recursos, tickets e impresión USB directa.
- `Management`: licencias, creación de negocios y simulación de un negocio de prueba.
- `tests`: pruebas del ORM, esquema hostelero y servicio de ventas.

## Hardware de impresión

NexoBar enumera y utiliza exclusivamente dispositivos USB mediante libusb. No integra Winspool,
CUPS ni colas de impresión del sistema operativo. Los perfiles del fabricante deciden el lenguaje,
el ancho de papel y, cuando existe, el pulso de apertura del cajón.

## Construcción

Las dependencias están declaradas en `vcpkg.json`. Los presets `ci-windows` y `ci-linux` se usan
tanto localmente como en GitHub Actions. Los ejecutables instalables se generan con:

```text
cmake --preset ci-windows
cmake --build --preset ci-windows
cmake --install build --config Release --prefix package
```

En Linux se sustituye `ci-windows` por `ci-linux`.
