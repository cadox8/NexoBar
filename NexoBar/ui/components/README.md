# Componentes de interfaz

La API está en el espacio global, sin namespaces. Todos los componentes se comparten con alias `XPtr` (`ComponentPtr`,
`ButtonPtr`, `FontPtr`, etc.). Las pantallas derivan de `IScreen`; `ScreenManager` mantiene la pantalla actual y una pila de
pantallas superpuestas.

Los componentes están agrupados por función: `assets`, `box`, `button`, `controls`, `core`, `image`, `layout`, `product` y `text`.
El arranque muestra `SplashScreen`, precarga los recursos desde el hilo principal y cambia de pantalla tras tres segundos. El logo
de inicio se carga desde `resources/logo/NexoBar.png`.

## Layout sencillo y zonas persistentes

```cpp
auto sale = std::make_shared<BlockComponent>(LayoutAxis::Horizontal);
auto catalogue = std::make_shared<BlockComponent>(LayoutAxis::Vertical, Color{30, 36, 48, 255});
auto ticket = std::make_shared<ScrollableBlock>(LayoutAxis::Vertical, Color{42, 48, 60, 255});

sale->content.gap(12).padding(LayoutInsets::all(12));
sale->addWeighted(catalogue, 3); // 3/4 del ancho flexible
sale->addWeighted(ticket, 1);    // 1/4; permanece al cambiar catálogo

catalogue->content.gap(8).padding(LayoutInsets::all(8));
catalogue->add(std::make_shared<StaticText>("Productos"), LayoutSize::fill(), LayoutSize::px(36));
std::weak_ptr<BlockComponent> weakCatalogue=catalogue;
auto changeView=std::make_shared<Button>("Cambiar a categorías", [weakCatalogue] {
    if(auto catalogue=weakCatalogue.lock()){
        catalogue->content.clear();
        catalogue->add(std::make_shared<StaticText>("Categorías"), LayoutSize::fill(), LayoutSize::px(36));
        // Añadir aquí los botones de categorías. El bloque del ticket no se modifica.
    }
});
catalogue->add(changeView, LayoutSize::fill(), LayoutSize::px(48));

ticket->add(std::make_shared<StaticText>("Ticket actual"), LayoutSize::fill(), LayoutSize::px(36));
```

El reparto también se puede escribir con `sale->addWeighted(catalogue, 3)` y `sale->addWeighted(ticket, 1)`. Para crear una
pantalla, deriva de `ScreenBase` y asigna `root = sale`; el área raíz sigue automáticamente el tamaño de la ventana.
`ScreenManager::set(std::make_shared<SalesScreen>())` activa la pantalla y `push/pop` mantiene pantallas superpuestas.

Las medidas también aceptan `LayoutSize::percent(0.25f)` y píxeles con `LayoutSize::px(120)`. Para un grid con spans:

```cpp
Layout grid;
grid.grid({1, 1, 1}, {2, 1, 1}).gap(8, 8).padding(LayoutInsets::all(8));
grid.addAt(button, 0, 0, 1, 2); // primera fila, ocupa dos columnas
grid.add(image, {});            // los demás hijos llenan las celdas libres en orden
```

Cada bloque contiene su propio `Layout`, por lo que se pueden anidar filas, columnas y grids. Hay padding, márgenes por hijo,
alineación, justificación, límites mínimo/máximo y modo debug con contornos.
`insert`, `replace`, `remove`, `at` y `optionsAt` permiten cambiar partes del árbol sin reconstruir el resto. Una instancia de
componente solo puede pertenecer a una celda; comparte datos/callbacks o crea otra instancia si necesitas mostrarlo dos veces.

## Catálogo, ticket y controles de caja

```cpp
auto products = std::make_shared<ScrollableBlock>(LayoutAxis::Vertical, Color{30, 36, 48, 255});
products->content.grid({1, 1, 1, 1}, {1, 1, 1}).gap(10).padding(LayoutInsets::all(10));
products->content.add(std::make_shared<ProductCard>("Café", "1,50 €", [] { /* añadir al ticket */ }));

auto quantity = std::make_shared<QuantityStepper>();
quantity->minimum = 1;
quantity->value = 2;
quantity->onChanged = [](int value) { /* actualizar unidades */ };
auto price = std::make_shared<MoneyText>();
price->amountMinor = 350; // 3,50 € con dos decimales
```

`ProductCard` es una baldosa de catálogo con imagen, nombre, detalle, precio y estado agotado. `QuantityStepper` sirve para
modificar unidades de líneas del ticket; `MoneyText` trabaja con unidades menores enteras para evitar errores de redondeo.
`CheckBox` y `Spacer` cubren opciones de caja y espacios flexibles. Los componentes aceptan una fuente desde `AssetManager`.

## Controles

- `Button`: texto, imagen, fuente, colores para hover/pulsado/deshabilitado, borde, esquinas redondeadas, sonido y callbacks.
- `ImageButton`: cambia entre texturas normal y hover.
- `DropdownComponent`: opciones con etiqueta/valor, índice seleccionado, teclado, ratón, rueda y callback de selección.
- `StaticText`: fuente, alineación, color y ajuste de texto.
- `InputText`: foco, UTF-8, máximo de caracteres, cursor, borrar, contraseña, multilínea y callbacks.
- `StaticImage`: ajustar, recortar o estirar textura.
- `ProductCard`, `QuantityStepper`, `MoneyText` y `CheckBox`: controles habituales de catálogo, ticket y caja.
- `Spacer`: hueco invisible que participa en el reparto del layout.
- `BoxComponent`, `RoundedBoxComponent`, `HoverBoxComponent`, `HoverRoundedBoxComponent`.
- `BlockComponent` y `ScrollableBlock`.

## Recursos precargados

```cpp
app.assets().registerTexture("logo", "logo.png");
app.assets().registerFont("normal", "normal.ttf", 24);
app.assets().registerSound("click", "click.wav");
app.assets().registerMusic("theme", "theme.ogg");
```

`NexoBar::init()` descubre automáticamente los ficheros de `resources/assets`, `resources/fonts`, `resources/sounds` y
`resources/music`, usando el nombre o la ruta relativa como clave. También puedes registrar claves manuales. El splash llama
`preloadNext()` en el hilo principal de Raylib y carga un recurso por frame. Al finalizar, la aplicación conserva la caché de
`shared_ptr` para accesos posteriores rápidos. La fuente integrada de Raylib está disponible como
`app.assets().getFont("default")`.

Los nombres de los recursos son la ruta relativa dentro de su carpeta sin extensión; por ejemplo `catalogo/bebidas` para
`resources/assets/catalogo/bebidas.png`. El acceso perezoso `getTexture/getFont/getSound/getMusic` carga y cachea la clave si
todavía no se había precargado. Las llamadas de carga y liberación de Raylib deben hacerse con el contexto/dispositivo activo.

Libera los componentes y llama `AssetManager::clear()` antes de cerrar la ventana o el dispositivo de audio. `NexoBar::shutdown()`
ya libera la caché en ese orden.
