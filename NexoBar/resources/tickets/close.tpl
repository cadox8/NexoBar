{{!block:business}}
{{business.name}}
{{!block:close_header}}
{{close.title}}
{{close.date}}
{{!block:separator_top}}
{{separator}}
{{!block:products}}
{{#each products}}{{close.product_row}}
{{/each}}{{!block:tickets}}
{{#each tickets}}{{close.ticket_row}}
{{/each}}{{!block:separator_total}}
{{separator}}
{{!block:close_summary}}
{{close.ticket_count_row}}
{{close.tax_base_row}}
{{close.tax_amount_row}}
{{close.total_row}}
{{close.cash_row}}
{{close.card_row}}
{{close.opening_amount_row}}
{{close.expected_amount_row}}
{{close.counted_amount_row}}
{{close.difference_row}}
{{close.movements_in_row}}
{{close.movements_out_row}}
{{!block:separator_bottom}}
{{separator}}

