# Guía de desarrollo de NexoBar

Esta guía describe la arquitectura activa, el recorrido de los datos y los puntos previstos para ampliar el proyecto. `old_` y
`Management` contienen código auxiliar; la aplicación gráfica está en `NexoBar` y la infraestructura compartida en `Platform`.

## Mapa del repositorio

```text
NexoBar/
├── Platform/
│   ├── database/                  ORM Nearon, SQLite, consultas y modelos
│   ├── database/hospitality/      Esquema y reglas comunes de hostelería
│   ├── i18n/                      Traducciones
│   ├── license/                   Licencias
│   └── time/                      UTC y zona horaria
├── NexoBar/
│   ├── NexoBar.h/.cpp             Ciclo de vida y servicios compartidos
│   ├── sales/SaleService.*        Emisión transaccional de tickets
│   ├── printer/                   Detección, perfiles y transporte RAW
│   ├── template/                  Renderizado de ticket.tpl
│   ├── ui/components/             Controles reutilizables
│   ├── ui/screen/sales/           TPV, catálogo, pedido y productos
│   ├── ui/screen/settings/        Vistas de ajustes
│   └── resources/                 Fuente, idiomas, imágenes y plantilla
├── tests/                         ORM, esquema y emisión de ventas
└── docs/                          Documentación técnica y operativa
```

## Arranque y servicios

`main.cpp` crea `NexoBar`, registra la instancia en `NexoBarImp` y llama a `run()`. `NexoBar::init()` prepara recursos,
traducciones, SQLite y copias automáticas. `run()` abre raylib, carga Inter como fuente `default` y monta splash, estado de carga
y login. Al elegir usuario se crea `ProductState` y se monta `MainScreen`.

`NexoBarImp` permite a las pantallas acceder a base de datos, traducciones, recursos, negocio y fuente global. El acceso siempre
se realiza mediante una instancia local (`NexoBarImp nexo; nexo->defaultFont()`); no existe un getter estático público.

## Base de datos y ORM

Los modelos están en `Platform/database/models`. Cada uno declara su `Nearon::Table`. Los identificadores son tipos fuertes:
construye `UserId{valor}` y extrae el entero con `.get()`.

```cpp
auto active = db.query<Product>()
    .where(Nearon::col<&Product::active> == true)
    .orderBy(Nearon::col<&Product::name>.asc())
    .all();

auto product = db.get<Product>(productId);
product.name = "Nuevo nombre";
db.update(product);
```

Usa `database.transaction([&] { ... })` cuando varias escrituras formen una operación. Una excepción revierte todo. Tickets,
líneas, impuestos y registros fiscales tienen triggers de inmutabilidad; una corrección futura debe crear un documento
rectificativo.

`initializeSchema()` crea las tablas y carga permisos, roles, administrador, impuestos y categorías. Para añadir un modelo,
inclúyelo en `Models.h` y en `createTables` dentro de `HospitalityDatabase.cpp`.

## Usuario activo y permisos

`LoginScreen` muestra los usuarios activos; `ProductState` recibe el usuario elegido y guarda `userId` y `roleId`.
`hasPermission(key)` consulta `permissions` y `role_permissions`. `MainScreen` habilita cada destino según los permisos efectivos.

| Clave | Uso |
|---|---|
| `sell` | Añadir productos e importes |
| `close_list` | Cobro y cierre |
| `manage_products` | Catálogo |
| `manage_users` | Empleados, roles y permisos |
| `manage_cash` | Caja y cierre diario |
| `view_reports` | Histórico |
| `configure_business` | Negocio y plantilla |
| `export_fiscal` | Exportación fiscal futura |

Permisos solo crea o elimina asociaciones `RolePermission`; nunca crea ni borra permisos del sistema. Empleados y roles tienen
alta, edición, activación/desactivación y borrado. Las claves foráneas protegen registros históricos referenciados.

## Flujo de venta

`ProductState::reloadCatalog()` lee categorías, productos e impuestos activos. `SelectedProductsScreen` mantiene el pedido
editable. Al pulsar **Imprimir**:

1. Convierte el pedido a `SaleItem` y obtiene el IVA del catálogo.
2. `SaleService::issue()` abre una transacción.
3. Busca o crea la serie anual `S`, incrementa `lastNumber` y forma `S-AAAA-NNNNNN`.
4. Calcula líneas y agrupa base e IVA por tipo usando céntimos enteros.
5. Inserta `Ticket`, `TicketLine`, `TicketTax` y auditoría.
6. Confirma y limpia el pedido editable.
7. Renderiza `ticket.tpl` y lo envía a la impresora.

La persistencia ocurre antes del hardware. Si imprimir falla, `m_pendingPrint` conserva el ticket ya emitido; el siguiente clic lo
reimprime sin reservar otro número ni duplicar la venta.

Para incorporar cobro, crea o localiza una `CashSession` y persiste `Payment` enlazado a una `ConsumptionList`. El esquema separa
documento, consumo, pago y caja; no añadas el método de pago directamente a `Ticket`.

## Interfaz y layouts

`MainScreen` monta cabecera, menú lateral y contenido; `ProductScreen` contiene únicamente el área de venta. Catálogo y pedido
siguen vivos al alternar categorías/productos. `ScreenLayout::setScreen()` sustituye una vista dentro de un slot.

- `LayoutSize::fill(peso)` reparte espacio.
- `LayoutSize::px(valor)` fija filas compactas.
- `ScrollableBlock` añade recorte y desplazamiento.
- `SettingsActionRow` presenta varios botones iguales en una fila.

Evita `DrawText` para la interfaz normal. Usa los componentes existentes o `DrawTextEx` con la fuente global.

## Ajustes

```text
SettingsHomeScreen
├── StaffSettingsScreen
│   ├── SettingsWorkspaceScreen(Users)
│   ├── SettingsWorkspaceScreen(Roles)
│   └── RolePermissionsScreen (checkboxes con scroll)
├── SettingsWorkspaceScreen(Business)
├── SettingsWorkspaceScreen(Template)
├── SettingsWorkspaceScreen(Status)
└── SettingsWorkspaceScreen(History)
```

`SettingsHomeScreen` es la portada compacta, con varias opciones por fila. Cada opción crea una vista distinta.
`SettingsWorkspaceScreen` comparte marco visual, scroll y constructores de controles, pero cada `Kind` monta contenido propio.

Para añadir una opción: añade un `Kind`, crea su método `build...()`, añade la tarjeta en `SettingsHomeScreen` y aplica el permiso
correspondiente antes de mostrar y ejecutar acciones sensibles.

## Impresión y plantilla

`PrinterDiagnostics` enumera colas USB. En Windows obtiene nombre, puerto, driver, papel y resolución desde
`PRINTER_INFO_2/DEVMODE`. Nombre y driver seleccionan ESC/POS o Star Line Mode. El papel compacto usa 32 columnas y el de 80 mm,
48.

`TicketPrinter` carga del perfil inicialización, corte y apertura de cajón. Estado ofrece pruebas de ticket, cajón y ambas juntas.
El ticket de prueba usa la plantilla real. La plantilla está en `resources/tickets/ticket.tpl`; la referencia de placeholders está
en `NexoBar/resources/tickets/PRINTERS_AND_PLACEHOLDERS.md`.

## Histórico y cierre

Facturación consulta tickets y pagos por día, mes o histórico completo. El informe contiene recuento, base, IVA, total,
efectivo, tarjeta y detalle. **Cerrar día e imprimir** exige periodo diario, imprime y cierra sesiones abiertas de la fecha,
calculando el efectivo esperado.

## Recursos e idiomas

Los catálogos están en `NexoBar/resources/languages`. Usa `app->text("grupo.clave", "Fallback")` y añade la misma clave a español
e inglés. Los recursos se copian junto al ejecutable tras compilar.

## Compilación y pruebas

En un terminal con Visual Studio cargado:

```powershell
cmake --build cmake-build-debug --config Debug -j 4
ctest --test-dir cmake-build-debug --output-on-failure -C Debug
```

Las pruebas cubren ORM, esquema y emisión de tickets. Para impresión sigue `tests/PRODUCTION_ACCEPTANCE.md`, ya que cola RAW,
corte y cajón requieren hardware.

## Reglas para ampliar

1. Mantén dinero en céntimos y tipos en puntos básicos; no persistas `double`.
2. Conserva la inmutabilidad de documentos emitidos.
3. Agrupa escrituras dependientes en transacciones.
4. Comprueba permisos al mostrar y ejecutar acciones sensibles.
5. Usa la fuente global y los componentes existentes.
6. Mantén una responsabilidad por vista o servicio.
7. Añade pruebas al cambiar numeración, impuestos, totales o relaciones.
8. Compila y ejecuta `ctest` antes de entregar.
