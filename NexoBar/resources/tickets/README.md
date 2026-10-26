# Plantillas de ticket

La plantilla `ticket.tpl` se imprime con un ancho predeterminado de 42 columnas y dos columnas de margen a cada lado. Se puede
pasar otro `ReceiptLayout` al generador para adaptar ancho y márgenes a la impresora:

```cpp
ReceiptLayout layout(48, 3, 3);
auto text = TicketTemplate::renderFile(path, ticket, lines, businessName, i18n, footer, layout);
```

Los tokens disponibles son `{{BUSINESS_NAME}}`, `{{TICKET_ROW}}`, `{{DATE_ROW}}`, `{{LINES}}`, `{{TOTAL_ROW}}`, `{{SEPARATOR}}` y
`{{FOOTER}}`. Se pueden ordenar, quitar o repetir. Para añadir contenido escrito directamente y alinearlo hasta la última columna
dejando el margen derecho, usa `{{RIGHT:Texto personalizado}}`.

En C++, `ReceiptLayout::row` acepta tantas columnas como hagan falta. El ancho de cada columna se indica en caracteres; la última
columna ocupa el espacio restante. Su alineación puede ser izquierda, centrada o derecha. `leftRight(izquierda, derecha)` sirve
para una línea flexible con el importe ajustado al margen final.
