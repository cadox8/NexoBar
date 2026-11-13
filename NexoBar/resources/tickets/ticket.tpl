{{BUSINESS_NAME}}
{{business.legal_name}}
{{business.tax_id}}
{{business.address}}
{{business.postal_code}} {{business.city}}
{{business.phone}}
{{business.email}}
{{separator}}
{{receipt.ticket_label}}: {{ticket.number}}
{{receipt.date_label}}: {{ticket.date}}
{{separator}}
{{#each lines}}{{line.row}}
{{/each}}{{separator}}
{{receipt.total_label}}: {{ticket.total}}

{{ticket.qr}}
{{ticket.footer}}
