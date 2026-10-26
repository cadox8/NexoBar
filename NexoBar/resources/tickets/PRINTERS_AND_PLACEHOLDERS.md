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

## USB printer discovery and diagnostics

The sales screen renders and logs the ticket through `TicketTemplate::printFile`, then submits a single RAW job containing the
drawer pulse and receipt. Only USB backed queues are considered: Windows USB ports (`USB...`) and Linux CUPS `usb://` device URIs.
If exactly one USB printer is present it is used even if the OS default points elsewhere; if several are present the USB default
must identify one. Linux deployments need the CUPS client tools installed. The queue must accept ESC/POS RAW data.

In Debug builds, Ajustes has a switch to show a scrollable ticket preview after each print attempt. Release builds compile that
switch out and force preview mode off. The preview strips ESC/POS control bytes for readability; the printer receives the original
bytes.
