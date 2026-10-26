# Nearon: wrapper SQLite tipado

`Nearon` es la capa de persistencia de `Platform`. Envuelve una conexión SQLite y ofrece un mapeo explícito entre structs/clases
C++ y tablas, parámetros enlazados, consultas tipadas y transacciones. No intenta descubrir automáticamente los miembros de una
clase: cada campo persistido se declara en un `Table`.

El dominio del bar vive en `Platform/database`: la capa SQL y SQLite está en la raíz, las operaciones específicas del negocio
están en `hospitality` y cada entidad está como clase independiente en `models`. No hay mesas ni comandas de restaurante. El bar
trabaja con listas de consumo asociadas opcionalmente a un cliente o a un nombre/etiqueta de grupo.

## Modelo del bar

- `Permission`, `Role`, `RolePermission` y `User`: permisos asignables por rol y usuarios de barra sin contraseña en el modelo.
  El usuario inicial es `Admin`; debe configurarse para el uso real.
- `Business`: perfil fiscal del negocio ya existente en el proyecto.
- `Category`, `Tax` y `Product`: catálogo de bebidas/café y tipos impositivos. `Product::openPrice` permite productos como
  “copa premium” cuyo importe se introduce al vender; también se puede añadir una línea sin producto mediante `addManualLine`.
- `Customer`, `ConsumptionList` (`lists`) y `ListLine`: una lista representa lo que consume un cliente o grupo, sin reservar
  mesa. Cada línea guarda descripción, cantidad, precio y tipo impositivo aplicados en ese momento. El esquema relaciona el
  futuro ticket con la lista; la emisión y cierre transaccional deben implementarse en la capa de operaciones.
- `CashSession`, `CashMovement` y `Payment`: apertura/cierre y arqueo de caja, entradas/salidas manuales y pagos en efectivo,
  tarjeta u otros medios.
- `TicketSeries`, `Ticket`, `TicketLine` y `TicketTax`: historial documental con numeración, datos del emisor y desglose de
  impuestos copiados al emitir. Las filas históricas tienen triggers que impiden actualizarlas o borrarlas. Una anulación se
  representa con un nuevo ticket rectificativo relacionado con el original, nunca editando el ticket emitido.
- `FiscalRecord`, `FiscalEvent` y `AuditEvent`: persistencia prevista para registros de facturación/eventos y auditoría. La
  existencia de estas tablas y de hashes almacenados no implementa por sí sola el sistema NO VERI*FACTU; ver `docs/FISCAL_ES.md`.

Operaciones incluidas hoy en `hospitality/HospitalityDatabase.h`:

```cpp
platform::I18n i18n("NexoBar/resources/languages");
auto db = nexobar::openHospitalityDatabase("data/bar.sqlite", i18n);
// initializeSchema es idempotente y se ejecuta al abrir.
auto line = nexobar::addProductToList(*db, listId, productId, 1000, userId);
// Para un producto marcado openPrice, pasa su precio de venta en céntimos como último argumento.
auto manual = nexobar::addManualLine(*db, listId, "Ronda", 1250, taxId, userId, i18n);
const auto total = nexobar::recalculateList(*db, listId);
```

## Idiomas

`Platform/i18n/I18n.h` expone `platform::I18n`, que carga catálogos JSON con claves por ruta, por ejemplo
`i18n.get("roles.manager.name")`. En Windows el idioma se detecta desde la lista de idiomas preferidos de la interfaz; si no
existe un catálogo para ese locale, se usa `en-US.json`. Cada archivo debe contener un objeto raíz `messages`.

NexoBar crea y conserva el servicio como `std::unique_ptr<platform::I18n>` y lo expone mediante `NexoBar::i18n()`. Los archivos de
`NexoBar/resources/languages` se copian junto al ejecutable a `resources/languages`. `HospitalityDatabase` recibe ese servicio al
inicializarse para guardar permisos, roles, usuario inicial, impuestos y categorías en el idioma seleccionado. Esos textos solo
se insertan si la tabla está vacía: cambiar el idioma después no modifica datos existentes ni los nombres que el negocio haya
personalizado. Los valores internos estables (`CASH`, `CARD`, claves de permisos y códigos de estado) permanecen en inglés.

Los errores de dominio usan claves (`HospitalityError::translationKey()`), que se pueden traducir al presentarlos:
`i18n.get(error.translationKey())`. Así la lógica de persistencia no decide cómo se dibujan los mensajes en la interfaz.

Los importes se guardan en céntimos y las cantidades en milésimas. Los tipos impositivos de ejemplo (21%, 10%, 4%, exento)
son semillas de catálogo, no una recomendación fiscal: el negocio debe configurar los aplicables a cada producto.

```cpp
#include "database/hospitality/HospitalityDatabase.h"

platform::I18n i18n("NexoBar/resources/languages");
auto db = nexobar::openHospitalityDatabase("data/bar.sqlite", i18n);
```

## Abrir la base de datos

```cpp
#include "database/Orm.h"

auto db = std::make_shared<Nearon::Database>("data/pos.sqlite");
db->createTables<Category, Product>();
```

`Database` posee la conexión SQLite. `OpenOptions` permite elegir modo de solo lectura, creación, claves foráneas, WAL y tiempo de
espera ante bloqueos. Por defecto activa claves foráneas, WAL para archivos y un timeout de 5 segundos. `":memory:"` crea una base
en memoria.

## Declarar un registro

Cada registro debe ser default-constructible para que el mapeador pueda leer columnas en sus miembros. El tipo de la clave es el
tipo real del miembro `PrimaryKey`, incluido `fluent::NamedType`:

```cpp
namespace app {
    using ProductId = fluent::NamedType<std::int64_t, struct ProductIdTag,
                                        fluent::Comparable, fluent::Hashable>;

    struct Product {
        ProductId id{};
        std::string name;
        std::int64_t priceCents{};
        std::optional<std::string> barcode;
    };

    using ProductTable = Nearon::Table<"products", Product,
        Nearon::Column<"id", &Product::id, Nearon::PrimaryKey, Nearon::AutoIncrement>,
        Nearon::Column<"name", &Product::name, Nearon::Unique>,
        Nearon::Column<"price_cents", &Product::priceCents,
                       Nearon::Check<"price_cents >= 0">>,
        Nearon::Column<"barcode", &Product::barcode>>;
    NEARON_TABLE(ProductTable);
}
```

`NEARON_TABLE` should be declared in the record's namespace so ADL can find it. It also supports a table descriptor inside a class
via `NEARON_TABLE_FRIEND`, which allows private members. Exactly one column must be marked `PrimaryKey`. `AutoIncrement` is
optional and only valid for integer storage keys. A rowid key with value zero is generated by SQLite on `insert`.

Supported built-in values include integral and floating-point numbers, bool, strings, blobs, enums, `std::optional<T>`,
`fluent::NamedType`, Boost date and timestamp types. Unnamed enums are stored as integers; enums with `nearonEnumNames` are stored
as text. A domain value such as `Money` needs a `Nearon::ValueCodec<T>` specialization that converts it to and from a supported
storage type.

## Insertar, leer y modificar

```cpp
app::Product product{app::ProductId{0}, "Café", 150, std::nullopt};
const app::ProductId id = db->insert<app::Product>(product);
// insert actualiza product.id cuando se genera una clave rowid.

std::optional<app::Product> found = db->select<app::Product>(id);
app::Product required = db->get<app::Product>(id); // lanza NotFoundError si falta

required.priceCents = 175;
bool changed = db->update<app::Product>(required);
db->upsert<app::Product>(required); // inserta o reemplaza las columnas no clave
bool removed = db->remove<app::Product>(id);
```

La conexión ejecuta las operaciones serializadas con un mutex recursivo. La sobrecarga `insert(const T&)` permite insertar un
temporal o un objeto const y devuelve la clave, pero no puede escribir la clave generada de vuelta en ese objeto. Para acceder a
ella, conserva el valor retornado o pasa un objeto mutable. `upsert` requiere una clave no nula; para crear filas con clave rowid
cero usa `insert`, que devuelve la clave asignada.

Las inserciones por lotes usan una transacción:

```cpp
std::vector<app::ProductId> ids = db->insertMany<app::Product>(products);
```

## Consultas

Las expresiones se forman a partir de miembros mapeados. Varias llamadas `where` se combinan con `AND`; `&&` y `||` combinan
condiciones. Los valores se enlazan como parámetros SQLite:

```cpp
using Nearon::col;
auto available = db->query<app::Product>()
    .where(col<&app::Product::priceCents> >= 100)
    .where(col<&app::Product::barcode>.isNotNull())
    .orderBy(col<&app::Product::name>.asc())
    .limit(20)
    .all();

auto one = db->selectFirst<app::Product>(col<&app::Product::name>.like("Caf%"));
auto count = db->count<app::Product>(col<&app::Product::priceCents> < 500);
auto total = db->sum<&app::Product::priceCents>();
```

Un `Field` permite `==`, `!=`, comparaciones de orden, `between`, `in`, `isNull`, `isNotNull`, `like`, `asc` y `desc`.
`field<Record, "nombre_sql">` permite referir una columna por su nombre SQL. La consulta base ofrece `all`, `first`, `count`,
`exists` y `sql`.

### Joins

Los joins devuelven una tupla con un registro por tabla. Las partes de un `LEFT JOIN` son `std::optional<T>`:

```cpp
auto rows = db->query<ConsumptionList>()
    .join<ListLine>(col<&ListLine::listId> == col<&ConsumptionList::id>)
    .leftJoin<Product>(col<&ListLine::productId> == col<&Product::id>)
    .where(col<&ConsumptionList::status> == ListStatus::Open)
    .all();
// tipo aproximado: vector<tuple<ConsumptionList, ListLine, optional<Product>>>
```

También existe el atajo `db->join<A, B>(condición)` y `db->leftJoin<A, B>(condición)`. Los joins se encadenan y sus condiciones
pueden referir campos de cualquiera de las tablas involucradas.

### UNION

`unionWith` produce `UNION` (elimina duplicados) y `unionAll` conserva duplicados. Ambas ramas deben consultar el mismo tipo de
registro y usar la misma conexión:

```cpp
auto results = db->query<Product>()
    .where(col<&app::Product::priceCents> < 100)
    .unionAll(db->query<Product>()
        .where(col<&app::Product::name>.like("Menú%")))
    .all();
```

No se permite unir consultas de conexiones diferentes: SQLite ejecutaría todo en una sola conexión y la segunda rama podría leer
otra base por error. Una consulta con `UNION` no se puede convertir en un join.

## Actualizaciones por condición y SQL parametrizado

```cpp
std::size_t updated = db->update<app::Product>()
    .set(col<&app::Product::priceCents>, 175)
    .where(col<&app::Product::id> == id)
    .exec();
```

Para expresiones SQL calculadas existe `raw`, por ejemplo `set(Nearon::raw("\"stock\" = \"stock\" - ?", 1))`. El texto SQL de
`raw` debe ser constante o de confianza; sus argumentos sí se enlazan como parámetros. No interpolar texto de usuario en SQL.
`removeWhere` sin condición elimina todas las filas de la tabla.

## Transacciones

```cpp
db->transaction([&] {
    db->insert<ConsumptionList>(list);
    db->insert<ListLine>(line);
});
```

Si la función retorna normalmente, se confirma; si lanza una excepción, se revierte. También se puede crear un objeto
`Transaction` manual y llamar a `commit()` o `rollback()`. Las transacciones anidadas usan savepoints. El objeto de transacción
retiene el mutex de la conexión durante toda su vida, por lo que otros hilos que utilicen esa misma conexión esperan hasta
terminar.

## Esquema y cambios

`createTables<T...>()` crea las tablas e índices declarados si faltan. Si una tabla ya existe, añade columnas nuevas que SQLite
permita añadir con `ALTER TABLE`; no elimina ni modifica columnas existentes y no convierte cambios incompatibles en migraciones
automáticas. No puede añadir una clave primaria a una tabla existente. Los cambios que requieran transformar datos deben
implementarse como migraciones explícitas, normalmente controladas con `userVersion()` y `setUserVersion()` dentro de una
transacción.

`Unique`, `Check`, `Default`, `Collate`, `ForeignKey`, `UniqueKey` e `Index` describen restricciones/índices. Las claves foráneas
SQLite están activadas por defecto en cada conexión. `dropTable<T>()` elimina la tabla y debe reservarse para operaciones de
mantenimiento controladas.

## Errores y límites

- `DatabaseError` representa errores SQLite o del wrapper; `ConstraintError` deriva de ella para restricciones; `NotFoundError` se
  usa por `get` cuando no encuentra un registro.
- `execute` ejecuta SQL sin parámetros. `run`, `fetch`, `scalar` y `queryRaw` cubren SQL manual; preferir parámetros enlazados.
- `update(record)` persiste todas las columnas salvo la clave primaria. Los registros fiscales inmutables del POS se protegen con
  triggers específicos del módulo POS, no por el ORM genérico.
- No hay carga automática de relaciones, seguimiento de cambios, caché de entidades ni conversión automática de esquemas desde la
  estructura C++.
- `Nearon` es una capa de acceso a datos. Su esquema o sus triggers por sí solos no acreditan cumplimiento fiscal. El estado y los
  requisitos pendientes del POS se describen en `docs/FISCAL_ES.md`.

## Archivos principales

- `Orm.h`: header de entrada para incluir Value, Codec, Schema, Expression, Connection, Query y Database.
- `Database.h`: API de registros, tablas, CRUD, agregados y transacciones.
- `Schema.h` y `Codec.h`: mapeo de registros/columnas, restricciones y tipos.
- `Expression.h` y `Query.h`: condiciones, orden, joins, unions y builders.
- `Connection.h/.cpp`: ciclo de vida SQLite, prepared statements, bloqueo, errores y savepoints.
