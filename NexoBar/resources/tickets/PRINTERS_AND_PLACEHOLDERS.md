# Ticket templates

`ticket.tpl` is plain UTF-8 text. The renderer supports arbitrary keys in the form `{{group.field}}` and repeating sections:

```text
{{#each lines}}
{{line.description}}
{{line.quantity}} x {{line.unit_price}}    {{line.total}}
{{/each}}
```

The built-in context includes `business.name`, `business.legal_name`, `business.tax_id`, `business.address`,
`business.postal_code`, `business.city`, `business.province`, `business.country`, `business.phone`, and `business.email`.

Ticket fields include `ticket.id`, `ticket.number`, `ticket.series`, `ticket.type`, `ticket.date`, `ticket.tax_base`,
`ticket.tax_amount`, `ticket.total`, `ticket.currency`, `ticket.customer_name`, `ticket.customer_tax_id`,
`ticket.customer_address`, `ticket.rectification_reason`, and `ticket.footer`.

Each `lines` item includes `line.id`, `line.number`, `line.description`, `line.quantity`, `line.unit_price`, `line.discount`,
`line.tax_rate`, `line.total`, and `line.row`. `line.row` is preformatted to the selected `ReceiptLayout` width. Existing tokens
such as `{{BUSINESS_NAME}}`, `{{TICKET_ROW}}`, `{{LINES}}`, and `{{TOTAL_ROW}}` remain supported.

Money values (`ticket.total`, `ticket.tax_base`, `ticket.tax_amount`, `line.unit_price`, `line.discount`, and `line.total`) are
already localized and include the currency suffix.

`TicketTemplate::render`, `renderFile` and `printFile` accept a `TemplateData` context. Add your own keys, including nested names,
and repeating sections. Custom keys override built-in keys with the same name. Unknown placeholders remain visible so template
mistakes are easy to spot:

```cpp
TicketTemplate::TemplateData custom;
custom.values["shop.slogan"] = "Gracias por su visita";
custom.values["cashier.name"] = cashierName;
TicketTemplate::TemplateData cash, card;
cash.values = {{"payment.method", "Efectivo"}, {"payment.amount", "12,50 €"}};
card.values = {{"payment.method", "Tarjeta"}, {"payment.amount", "4,00 €"}};
custom.sections["payments"] = {cash, card};
auto ticket = TicketTemplate::renderFile(path, invoice, lines, businessName, i18n,
                                         {}, ReceiptLayout{}, custom);
```

Then use `{{shop.slogan}}`, `{{cashier.name}}`, and `{{#each payments}}{{payment.method}}: {{payment.amount}}\n{{/each}}` in
`ticket.tpl`.

The touch editor does not keep its available-placeholder catalogue in UI code. Schema version 4 seeds the
`ticket_placeholders` table with the visible token, its translation key, the valid template fragment, order, and active state.
Descriptions are resolved from the embedded language JSON catalog, so database rows stay language-independent while the two editor
columns remain fully localized. Saved templates contain `{{!block:...}}` metadata comments to restore ordering; the renderer removes
these comments before previewing or printing.

## USB printer discovery and diagnostics

The sales screen renders and logs the ticket through `TicketTemplate::printFile`, then submits a single job containing the drawer
pulse and receipt directly to the USB bulk OUT endpoint through libusb. No Windows spooler or Linux CUPS queue is used. Printer-class
devices and known thermal-printer vendors are discovered automatically. The model/profile provides ESC/POS, StarPRNT, ZPL or Brother
raster commands and a 58/76/80/112 mm paper-width estimate. Linux requires an appropriate udev permission rule; Windows requires a
WinUSB/libusb-compatible device interface. `NEXOBAR_USB_PRINTER` can select a device by a substring of its displayed name.

In Debug builds, Ajustes has a switch to show a scrollable ticket preview after each print attempt. Release builds compile that
switch out and force preview mode off. The preview strips ESC/POS control bytes for readability; the printer receives the original
bytes.
