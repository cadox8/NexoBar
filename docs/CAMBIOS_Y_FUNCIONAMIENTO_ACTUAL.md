# NexoBar: cambios y funcionamiento actual

Documento de referencia de la refactorización de octubre de 2026.

## Estado de la solicitud

- [x] Base de pantalla componible con regiones reemplazables, flex, grid y capas.
- [x] Pantalla de carga con progreso, recurso actual, avisos y errores.
- [x] `SplashScreen` con logotipo y duración mínima exacta de tres segundos.
- [x] `MainScreen` modular con cabecera y menú lateral persistentes.
- [x] Cabecera `{bar} - {user}` y reloj en tiempo real siempre visibles durante la sesión.
- [x] Menú persistente con Venta, Reportes, Ajustes y Cerrar sesión.
- [x] Venta dividida entre catálogo navegable y ticket/teclado/impresión/cajón.
- [x] Alta, edición y baja lógica de categorías y productos.
- [x] Reportes por día, mes y año, con detalle, total e impresión del periodo.
- [x] Ajustes divididos en pantallas sustituibles para empleados, roles, permisos, negocio, plantilla, estado e histórico.
- [x] Acceso global exclusivamente mediante una instancia local `NexoBarImp nexo;`.
- [x] Nuevos componentes reutilizables y adaptables a flex/grid.
- [x] Cargadores y cachés independientes para texturas, fuentes, sonidos y música.
- [x] Catálogos de idioma embebidos en el ejecutable, con español como fallback.
- [x] Opción de Management para crear un negocio simulado funcional.
- [x] Permisos ampliados y asignación por rol mediante checkboxes en una vista con scroll.
- [x] Compilación Windows/Linux con manifiesto vcpkg y presets CMake.
- [x] Workflow de build/test para Windows y Linux.
- [x] Workflow de versión para compilar, empaquetar y publicar artefactos.
- [x] Detección e impresión USB directa, sin Winspool ni CUPS.
- [x] Detección de familia de comandos, endpoint, fabricante y ancho de papel.
- [x] Editor táctil de tickets mediante bloques reordenables y activables.
- [x] Catálogo de placeholders descriptivos disponible dentro del editor, sin exponer identificadores internos.
- [x] Revisión del esquema de hostelería, seeds idempotentes, permisos y datos históricos inmutables.

## Flujo de ejecución

1. `NexoBar` prepara internacionalización, recursos, base de datos, backups y licencia.
2. El splash muestra el logotipo durante al menos tres segundos mientras avanza la precarga.
3. Si quedan tareas se abre `LoadingScreen`; los fallos permanecen visibles y no dejan entrar en una sesión inválida.
4. `LoginScreen` muestra los usuarios activos. Al tocar uno se crea el estado de sesión con su rol.
5. `MainScreen` conserva `HeaderScreen` y `SideMenuScreen` mientras sustituye únicamente el contenido central.
6. Cerrar sesión destruye la vista de trabajo y vuelve a la selección de usuario.

## Pantallas y componentes

`ScreenBase` acepta componentes directamente y respeta el área donde se monte. `ScreenLayout` permite flujo horizontal o vertical, tamaños en píxeles/porcentaje/peso, límites mínimo/máximo, grid y regiones absolutas. Una subpantalla se cambia mediante su nombre sin reconstruir el marco principal.

Componentes añadidos:

- `ProgressBar`: progreso reutilizable.
- `NavigationButton`: botón seleccionable para navegación.
- `ModalComponent`: overlay adaptable con contenido reemplazable.
- `TicketTemplateBlockEditor`: bloques táctiles ordenables, eliminables y con scroll independiente.

La pantalla de venta sigue separando catálogo y pedido. El catálogo cambia de categorías a productos dentro de la misma región. La pantalla de reportes consulta tickets reales y calcula el resumen del día, mes o año seleccionado.

## Recursos e idiomas

`AssetManager` es una fachada. La propiedad y la caché real están separadas en `TextureLoader`, `FontLoader`, `SoundLoader` y `MusicLoader`; se mantienen los aliases `TexturePtr`, `FontPtr`, `SoundPtr` y `MusicPtr`. Los recursos principales entran en la precarga y los descubiertos se registran en modo lazy para no mantener fuentes, sonidos o música sin usar en memoria.

Los JSON `es-ES.json` y `en-US.json` se convierten durante CMake en `EmbeddedLanguages.h` tanto para NexoBar como para Management. La aplicación no depende de una ruta de idiomas en producción. Los textos visibles, validaciones y errores de hardware se resuelven por clave; el locale del sistema se intenta primero y cualquier clave ausente cae en `es-ES`.

## Base de datos, usuarios y permisos

La inicialización del esquema es idempotente. En cada arranque añade permisos nuevos por clave y actualiza sus descripciones traducidas, sin duplicarlos. El administrador inicial recibe todos los permisos; encargado y barra reciben conjuntos limitados. Se añadieron capacidades separadas para ticket, cajón, roles, asignación de permisos, impresión de reportes, impresora y plantilla.

La pantalla de permisos carga todos los permisos activos de un rol en checkboxes con una descripción localizada y comprensible, sin mostrar claves técnicas. Cada toque persiste inmediatamente la relación `RolePermission`. La lista es desplazable y el desplegable de roles se dibuja fuera del recorte del panel para mostrar todas sus opciones.

Los tickets, líneas, impuestos, registros fiscales, eventos fiscales y auditoría siguen protegidos por triggers contra `UPDATE` y `DELETE`. No se añadieron mesas, comandas ni gestión de salón.

Management abre el mismo esquema completo que NexoBar. La opción **Simular negocio** crea o actualiza un bar ficticio, licencia válida, catálogo, usuario/roles/permisos y dos ventas de muestra.

## Impresoras USB

La detección usa libusb y enumera interfaces de clase impresora o dispositivos de fabricantes térmicos conocidos. Se reconocen perfiles Epson, Star, BIXOLON, Citizen, Custom, Rongta, HPRT, POSBANK, SNBC, DATECS, Sewoo/LK, XPrinter-compatible, Zebra y Brother; cualquier dispositivo de clase impresora con endpoint bulk OUT entra como compatible genérico.

Se obtienen VID/PID, bus, dirección, fabricante, modelo, serie, interfaz y endpoint. El ancho se infiere del descriptor/modelo y del perfil (58, 76, 80 o 112 mm); el alto no se fija porque el rollo térmico es continuo. Los comandos implementados son ESC/POS, StarPRNT, ZPL y Brother raster. El pulso de cajón solo se habilita cuando el perfil lo soporta.

No se usan colas ni APIs de impresión del sistema. En Linux el usuario necesita permisos sobre el dispositivo USB mediante udev. En Windows la interfaz debe estar asociada a WinUSB/libusb; no se utiliza Winspool.

Si hay varios dispositivos, puede fijarse uno con `NEXOBAR_USB_PRINTER`, usando una parte del nombre mostrado. Sin configuración se elige determinísticamente el primero por bus y dirección.

## Editor de ticket

La plantilla se configura sin teclado con bloques de negocio, separador, datos de ticket, líneas, totales y pie. Los controles táctiles permiten subir, bajar, quitar y añadir bloques sin interferir con el scroll. Guardar vuelve a generar `ticket.tpl`.

Los placeholders se ofrecen en una columna seleccionable y debajo se previsualiza el resultado. Se mantienen los placeholders históricos en el renderizador para no romper plantillas existentes. No existen placeholders de mesa.

## Iteración de interfaz, sesiones y cierre

- [x] La hora dispone de más ancho y margen vertical para no quedar recortada.
- [x] Cerrar sesión ocupa siempre la última posición del menú.
- [x] Los iconos generales intentan cargarse desde SVG y disponen de PNG transparente equivalente como fallback para builds de raylib sin decodificador SVG; las imágenes de producto continúan en PNG.
- [x] Header y menú comparten un color exclusivo; el menú mide lo mismo que la altura del header y solo muestra iconos.
- [x] Las líneas del ticket tienen padding completo y un control de borrado más pequeño.
- [x] El bloque total tiene la misma altura que cajón/imprimir y el importe queda centrado.
- [x] La fuente incorpora el glifo euro y los importes muestran `€`.
- [x] Facturación se ha retirado de Ajustes y el informe completo vive en Reportes, con controles arriba y ticket debajo.
- [x] El ticket de cierre agrupa cada producto vendido y suma cantidades, importes y total diario.
- [x] El cierre se imprime desde Reportes y se intenta imprimir antes de salir; cerrar e imprimir cierra las sesiones de caja abiertas del día.
- [x] Reportes y Ajustes respetan los permisos de rol, incluida la impresión y el cierre.
- [x] Cambiar de empleado no elimina tickets: las ventas del día permanecen en la base de datos.
- [x] Los usuarios admiten PIN opcional de 4 a 6 dígitos, derivado con salt y editado mediante un campo enmascarado.
- [x] Al elegir un usuario protegido aparece un teclado numérico táctil que valida el PIN.
- [x] Los campos de Ajustes muestran un teclado virtual con entrada táctil y cambian al modo de teclado físico al escribir.
- [x] La barra espaciadora ya no activa botones de navegación mientras se escribe.
- [x] El editor de plantilla usa dos columnas, inserción táctil de placeholders, ordenación sin arrastre y muestra inferior.
- [x] Categorías y productos usan tres tarjetas por fila, tipografía mayor e imagen ampliada para aprovechar el espacio disponible.

El editor de tickets ya no depende del gesto de arrastre: los controles subir/bajar evitan el conflicto con el scroll. La columna derecha añade placeholders al final y la zona inferior representa un ticket de muestra. No se muestran explicaciones redundantes ni el antiguo indicador que podía aparecer como `?`.

El modo de entrada cambia con el dispositivo usado. `NEXOBAR_TOUCH_ONLY=1` fuerza el teclado táctil desde el arranque; una pulsación real pasa al modo de teclado físico.

## Compilación y publicación

Desarrollo local con vcpkg:

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

Build equivalente a CI:

```bash
cmake --preset ci-windows   # o ci-linux
cmake --build --preset ci-windows
ctest --preset ci-windows
```

`.github/workflows/build.yml` ejecuta build y tests en `windows-latest` y `ubuntu-latest`. `.github/workflows/release.yml` se activa con tags `v*` o manualmente, crea paquetes ZIP para Windows y Linux y los adjunta a GitHub Releases. En Windows, la instalación incorpora las DLL de ejecución del triplet de vcpkg para que el ZIP sea autocontenido.

## Verificación realizada

- Configuración y build Release completos con el preset `ci-windows`: NexoBar, Management y tests.
- 6/6 suites CTest correctas.
- Validación sintáctica de ambos catálogos JSON.
- El test de esquema valida los 18 permisos y que el administrador los recibe todos.
- El test de esquema reinicializa una base con un negocio completo y comprueba que sus datos identificativos y licencia se conservan.
- Instalación de prueba correcta, incluidos ejecutables, recursos y DLL de vcpkg necesarias.

## Corrección de layout y mensajes de error

- [x] Cada componente recibe su área también antes del primer dibujado, por lo que un cambio de pantalla dentro de `update()` ya no intenta construir categorías con ancho cero.
- [x] Categorías limita defensivamente el tamaño mínimo de sus celdas y nunca entrega medidas negativas al layout.
- [x] Los errores visibles son mensajes traducidos y orientados al usuario; las excepciones técnicas completas quedan únicamente en el log.
- [x] Se eliminan los prefijos redundantes `Error: Error inesperado:` y se evita registrar el mismo mensaje visible más de una vez.
- [x] La regresión está cubierta por `LayoutTests.PropagatesAreaBeforeFirstDraw`.

## Catálogo, venta y editor de ticket

- [x] Los nombres de categorías y productos tienen mayor tamaño y aprovechan mejor el espacio libre de las tarjetas.
- [x] Los botones de alta usan un icono vectorial de suma reutilizable en lugar del carácter `+`.
- [x] Las líneas de la venta tienen margen lateral, más separación vertical y tipografía mayor.
- [x] Cerrar sesión conserva siempre el color rojo, también en sus estados activo, hover y pulsado.
- [x] Cada impresión emite una venta nueva con el importe actual. Un fallo físico de impresora ya no reutiliza el ticket anterior ni su precio.
- [x] Después de persistir el ticket, la lista se vacía siempre para comenzar la siguiente venta, aunque no haya impresora USB conectada.
- [x] Se eliminan de la interfaz y de las APIs de venta el precio abierto y las categorías anidadas. La migración de esquema normaliza datos antiguos sin eliminar columnas de instalaciones existentes.
- [x] El esquema 4 añade `ticket_placeholders`, con placeholder, clave descriptiva traducible, contenido insertable, orden y estado.
- [x] El editor carga los placeholders desde la base de datos y muestra únicamente sus nombres traducidos; los tokens internos no se exponen.
- [x] Las columnas de contenido y placeholders tienen scroll independiente por rueda o gesto táctil, con barras visibles y sin paginación.
- [x] Cada bloque seleccionado muestra los dos controles triangulares de orden y un botón verde de eliminación totalmente visible.
- [x] La plantilla guarda metadatos de orden que no se imprimen y la vista previa diferencia negocio, líneas, total y pie mediante tamaños y espaciados propios de un ticket.

## Ajustes visuales y mantenimiento final

- [x] La flecha inferior del editor usa el mismo orden de vértices que la superior, evitando que el back-face culling de raylib la oculte.
- [x] El editor conserva claves de traducción en sus bloques y resuelve el texto desde el catálogo activo al dibujar; las filas personalizadas también admiten un nombre descriptivo literal guardado en base de datos.
- [x] Cada bloque, salvo el primero, ofrece un control táctil para unirlo o separarlo de la fila anterior; las uniones se guardan en la plantilla y se imprimen automáticamente como `bloque anterior - bloque unido`.
- [x] La vista previa representa las agrupaciones en la misma línea y el formato se conserva después de guardar y volver a abrir la plantilla.
- [x] CMake vigila los JSON de idioma y regenera automáticamente los catálogos embebidos exclusivamente para NexoBar cuando cambia cualquiera de ellos.
- [x] La imagen de `ProductCard` ocupa una proporción mayor de la tarjeta sin invadir el nombre ni el precio.
- [x] El cursor de los campos de texto mide lo mismo que la tipografía y queda alineado con la línea escrita.
- [x] Todos los botones del menú lateral tienen un fondo propio visible; la selección conserva un color diferenciado y cerrar sesión continúa en rojo.
- [x] El total reduce dinámicamente su tipografía según el ancho y alto disponibles para mostrar siempre la etiqueta, todos los dígitos y la moneda.
- [x] Los nombres de bloques, placeholders, permisos y estados visibles de NexoBar proceden de los JSON. Los catálogos español e inglés contienen las mismas 380 claves.
- [x] El desplegable de roles muestra la lista completa sobre el panel y mantiene scroll cuando hay más opciones que espacio.
- [x] La cabecera refresca el nombre y el rol de la sesión activa desde la base de datos sin reconstruir ni vaciar la venta actual.
- [x] La coordenada del último toque se conserva durante el evento de liberación, evitando pulsaciones desplazadas en controles táctiles.
- [x] Management dispone de `--initialize-defaults <ruta.db>` para actualizar seeds sin abrir el menú interactivo y verifica que todas las filas de negocio permanezcan idénticas.
- [x] Se ejecutó Management sobre `cmake-build-debug/bin/nexobar.db`; los datos predeterminados quedaron actualizados y el negocio existente pasó la comprobación de conservación.

## Bloqueo de inicio sin negocio

- [x] La ausencia de un negocio configurado es ahora un error de inicio bloqueante, no un aviso ni un modo de pruebas.
- [x] Tras los tres segundos del splash se muestra una pantalla de error traducida con la acción necesaria en Management.
- [x] Tanto el acceso al login como la apertura de una sesión comprueban el estado del negocio, por lo que no se puede entrar por una ruta alternativa.

## Auditoría completa de textos e idiomas

- [x] Se eliminan `errors.error_prefix` y `errors.exception_prefix`: los errores se muestran directamente, sin prefijos vacíos ni duplicados.
- [x] Los textos visibles que quedaban incrustados en C++ pasan a los catálogos: nombre de aplicación, controles de cierre, errores de venta, tipos de ticket y licencia, datos USB, unidades y formatos de ticket.
- [x] Los datos del negocio simulado y sus productos proceden también del idioma activo.
- [x] Los permisos se presentan por su descripción traducida y ya no exponen su clave interna.
- [x] NexoBar mantiene todos sus textos visibles en los catálogos; Management es deliberadamente una herramienta independiente solo en inglés y conserva sus textos en los `.cpp`.
- [x] `LocalizationTests` compara todas las claves de español e inglés, comprueba cada clave literal usada desde C++ y bloquea textos visibles incrustados en componentes de presentación.

## Management en inglés

- [x] Menús, formularios, validaciones, ayudas, resultados y salida CLI de Management están exclusivamente en inglés y definidos en sus `.cpp`.
- [x] Se elimina la sección `management` de ambos catálogos JSON para evitar fuentes de texto duplicadas.
- [x] Management no lee, incluye, genera ni analiza catálogos JSON. Los textos ingleses necesarios para sus datos iniciales también están definidos en `Management.cpp`.
- [x] La inicialización del esquema y las ventas reciben un proveedor de textos desacoplado: NexoBar utiliza su sistema de idiomas y Management utiliza exclusivamente sus literales internos.
- [x] Al completar la simulación se abre una vista de resultado con el aviso explícito `Simulation completed successfully` y el resumen de los datos preparados.

## Empaquetado local

- [x] `package_nexobar.bat` empaqueta la compilación Release de Windows.
- [x] `package_nexobar.sh` realiza el mismo proceso en Linux.
- [x] Ambos aceptan opcionalmente la ruta del directorio de build Release y generan `dist/nexobar`.
- [x] El paquete contiene NexoBar, Platform, las bibliotecas dinámicas realmente utilizadas y los recursos de ejecución; no incluye Management ni las bibliotecas de tests.
