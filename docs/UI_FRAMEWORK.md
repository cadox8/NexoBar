# UI de NexoBar con raylib y C++20

La interfaz se compone con contenedores flex y grid. Cada pantalla recibe un rectángulo de su padre mediante `IScreen::area()` y
recalcula sus controles en ese espacio. Los porcentajes y pesos flexibles permiten que el contenido se adapte al tamaño actual de
la ventana; los pocos tamaños fijos se reservan para objetivos compactos de navegación, como el header y el rail lateral.

## Estructura de la aplicación

`ProductScreen` monta un header de 44 px y, debajo, un rail de 44 px junto al área principal. El área de venta tiene exactamente
dos paneles: un catálogo de navegación y la lista del pedido. El panel de catálogo alterna entre categorías y productos con
`ScreenLayout::setScreen("catalog", vista)`. Categorías y productos mantienen instancias separadas; la lista de venta permanece
montada durante el cambio y conserva cantidades, importe manual, teclado numérico y total.

```text
ProductScreen
├── HeaderScreen (44 px)
└── Body
    ├── SideMenuScreen (44 px)
    └── Workspace
        ├── CatalogHost (CategoriesCatalogScreen ↔ ProductCatalogScreen)
        └── SelectedProductsScreen (persistente)
```

El rail no contiene marca ni texto: presenta iconos cuadrados de Venta arriba y Ajustes abajo. La cabecera muestra el negocio, el
usuario y el reloj. Ajustes cambia el contenido central, dejando siempre visible la navegación común.

## Flujo del catálogo

`ProductState` consulta categorías, productos e impuestos activos usando `Nearon::Database`. Al abrir una categoría, el host
sustituye solo la vista de categorías por `ProductCatalogScreen`; el botón de volver restaura las categorías. Tanto categorías
como productos usan `ProductCard`.

Los botones `+` y `−` del catálogo abren el alta contextual o desactivan el elemento seleccionado. El formulario de categoría usa
`name`, `icon` y `parent_id`; el de producto usa `name`, `category_id`, `tax_id`, `price_cents` y `open_price`. `active` se crea
como verdadero según el valor por defecto del modelo. El precio se introduce en moneda, se convierte a céntimos y admite coma o
punto decimal. El precio puede quedar vacío cuando se activa precio abierto.

La eliminación es lógica (`active = false`) para conservar las referencias históricas. Al eliminar una categoría también se
desactivan sus productos activos. Tras cada cambio, `ProductState` vuelve a consultar la base de datos y las tarjetas se
reconstruyen. Los artículos temporales de importe manual y las cantidades de productos que siguen activos se conservan en el
pedido.

## Medidas, escala y legibilidad

Header y rail usan medidas de 44 px a tamaño normal. Los paneles del espacio de trabajo se reparten con pesos flexibles 6:4; el
catálogo puede crecer con la ventana, mientras la lista de venta mantiene su zona. El contenido usa márgenes y huecos
proporcionales al viewport, tipografías con límites legibles y `ProductCard` con esquinas suaves. El botón «Añadir importe» tiene
un hueco propio respecto a la última fila del teclado numérico.

Raylib trabaja en coordenadas de ventana: el área, el puntero del ratón y el toque usan la misma referencia. `UIInput` entrega
eventos de puntero y navegación por teclado. Los botones distinguen hover real del puntero del foco de teclado, así el foco no
pinta el botón Venta como si el ratón estuviera encima. Campos, casillas y listas desplegables aceptan también el puntero táctil.

## Diálogo modal

`CatalogPopupScreen` es una capa por encima del contenido. Mientras está abierta, solo recibe entrada el formulario y bloquea
clics de las pantallas inferiores. El diálogo presenta los campos de base de datos adecuados al tipo de elemento, valida nombre,
categoría, impuesto y precio, y muestra errores sin cerrar el formulario.

## Añadir pantallas y componentes

Para añadir una vista, crea la pantalla y móntala en `ScreenLayout`. Usa `LayoutSize::fill(weight)` para reparto flexible,
`LayoutSize::percent()` para proporciones y `LayoutSize::px()` solo cuando exista una dimensión realmente fija. `setScreen()`
reemplaza una vista por otra manteniendo las opciones de layout registradas.

```cpp
auto workspace = std::make_shared<ScreenLayout>(LayoutAxis::Horizontal);
workspace->addWeighted("catalog", catalog, 6.f);
workspace->addWeighted("ticket", ticket, 4.f);
```

`BlockComponent` y `Layout` ofrecen el mismo modelo para controles. `ScrollableBlock` añade recorte y desplazamiento;
`ScreenLayout::grid(rows, columns)` y `addScreenAt()` permiten distribuciones en cuadrícula. Los anclajes relativos (`addRegion`)
sirven para overlays, como el diálogo modal.

## Archivos principales

- `../NexoBar/ui/screen/sales`: composición general, navegación y overlay modal.
- `../NexoBar/ui/screen/sales`: catálogo activo, persistencia y estado del pedido.
- `../NexoBar/ui/screen/sales`: tarjetas de categorías y acciones.
- `../NexoBar/ui/screen/sales`: tarjetas de productos y navegación de categoría.
- `../NexoBar/ui/screen/sales`: pedido, teclado de importe y total.
- `../NexoBar/ui/screen/sales`: alta de categoría y producto.
- `../NexoBar/ui/screen/menus`: cabecera compacta.
- `../NexoBar/ui/screen/menus`: rail de iconos Venta/Ajustes.
- `NexoBar/ui/components/product/ProductCard.h/.cpp`: tarjeta común del catálogo.
- `NexoBar/ui/screen/ScreenLayout.h/.cpp` y `NexoBar/ui/components/layout/Layout.h/.cpp`: layout flexible, grid y áreas
  adaptables.
