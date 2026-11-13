# NexoBar

NexoBar es un prototipo de terminal de punto de venta (TPV) táctil y multiplataforma para bares y pequeños negocios de hostelería. Está desarrollado en C++20 y reúne ventas, gestión de productos, usuarios y permisos, cierres de caja, personalización de tickets e impresión térmica USB directa en una aplicación de escritorio.

> [!WARNING]
> El proyecto está en desarrollo. No está homologado ni se declara conforme con Veri*Factu, el Real Decreto 1007/2023 o la Orden HAC/1177/2024. No debe utilizarse para facturación real sin completar y validar los requisitos fiscales, legales y de seguridad aplicables.

## Funciones principales

- Interfaz táctil construida con raylib y componentes propios.
- Catálogo de categorías y productos, venta activa y cuentas abiertas.
- Usuarios, roles y permisos configurables.
- Sesiones y movimientos de caja, informes y cierre diario.
- Tickets editables mediante plantillas y marcadores de posición.
- Impresión USB directa con detección de impresoras térmicas.
- Base de datos SQLite con copias de seguridad automáticas.
- Interfaz disponible en español e inglés.
- Herramienta de administración en terminal para preparar datos y licencias.
- Pruebas automatizadas del modelo de datos, ventas, fiscalidad, diseño y localización.

## Tecnologías

- C++20 y CMake 3.24 o posterior.
- vcpkg para gestionar dependencias.
- raylib para la interfaz gráfica y FTXUI para la herramienta de administración.
- SQLite, Boost, libusb, OpenSSL, LibXml2 y nlohmann/json.
- GoogleTest para las pruebas automatizadas.
- GitHub Actions para compilar y probar en Windows y Linux.

## Estructura del proyecto

```text
NexoBar/       Aplicación principal, interfaz, ventas, impresión y recursos
Platform/      Base de datos, seguridad, fiscalidad, licencias e internacionalización
Management/    Herramienta de administración en terminal
.github/       Flujos de integración continua y publicación
```

## Requisitos

- CMake 3.24+
- Ninja
- Un compilador compatible con C++20: MSVC en Windows o GCC en Linux
- Git
- Una instalación de vcpkg y la variable de entorno `VCPKG_ROOT`

En Linux también se necesitan los paquetes de desarrollo de X11, OpenGL, ALSA y udev que utiliza raylib y el acceso USB.

## Compilar y probar

Clona el repositorio con Git y sitúate en su directorio. En Windows, usa una consola de desarrollador de Visual Studio:

```powershell
cmake --preset ci-windows
cmake --build --preset ci-windows
ctest --preset ci-windows
```

En Linux:

```bash
cmake --preset ci-linux
cmake --build --preset ci-linux
ctest --preset ci-linux
```

Los ejecutables se generan en `build/bin`. El preset `dev` proporciona una compilación de depuración para desarrollo local en Windows.

## Ejecutables

- `NexoBar`: aplicación TPV con interfaz táctil.
- `Management`: utilidad de terminal para administrar licencias, negocios y datos de simulación.

La base de datos `nexobar.db` se crea al ejecutar la aplicación por primera vez. El esquema, los permisos y los datos iniciales se preparan de forma idempotente.

## Crear un paquete

Tras generar una compilación Release, los scripts de empaquetado copian el ejecutable, las bibliotecas necesarias y los recursos a `dist/nexobar`:

```powershell
.\package_nexobar.bat build
```

```bash
./package_nexobar.sh build
```

## Certificados y datos privados

El repositorio público no contiene identidades fiscales reales, certificados, claves privadas ni contraseñas. `SoftwareProducerIdentity` usa datos ficticios reservados para pruebas y mantiene desactivada la configuración de producción. Esos valores deben sustituirse de forma privada y revisarse antes de cualquier compilación destinada a producción.

Los certificados locales se cargan desde `NexoBar/resources/certs`, una ruta excluida de Git. No subas nunca archivos `.p12`, `.pfx`, `.pem`, `.key` ni sus contraseñas. Los secretos usados para firmar versiones en GitHub Actions deben almacenarse exclusivamente como secretos cifrados del repositorio.

## Estado y licencia

NexoBar es un proyecto experimental y todavía puede contener funciones incompletas o cambios incompatibles entre versiones.

Este repositorio no incluye por ahora una licencia de distribución. Hasta que se añada una, el código no se ofrece bajo una licencia de código abierto.
