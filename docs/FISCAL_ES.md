# Situación fiscal del prototipo y ruta de trabajo

## Estado

Este proyecto es un prototipo operativo y **no se declara conforme** al RD 1007/2023 ni a la Orden HAC/1177/2024. No debe utilizarse para expedir facturas reales hasta implementar y verificar los requisitos aplicables y completar la declaración responsable del productor para la versión concreta.

Los tickets impresos actualmente son tickets operativos de prueba. No deben presentarse como facturas fiscales ni se convierten en registros fiscales por enviarlos posteriormente a una gestoría.

## Modalidad seleccionada: NO VERI*FACTU

NO VERI*FACTU no significa que no haya requisitos fiscales. En esta modalidad deben implementarse, según las especificaciones vigentes, los registros de facturación de alta y anulación, su encadenamiento y huella, firma XAdES Enveloped Signature de los registros de facturación y de evento, registro de eventos, QR fiscal, conservación, exportación y herramientas para comprobar integridad y firma. La emisión de la factura y la generación del registro correspondiente no pueden separarse como un reproceso posterior.

No basta con un hash SHA-256 aislado, una tabla SQLite, un PDF con QR o un archivo enviado a la gestoría.

## Plan de implementación y validación

1. Acordar con la gestoría los tipos de documentos, los tipos de IVA, numeración y obligaciones de facturación del bar.
2. Implementar pruebas automatizadas que reproduzcan literalmente los vectores, ejemplos, esquemas y validaciones publicados por la AEAT. El plan está en `../tests/AEAT_CONFORMANCE_TEST_PLAN.md`.
3. Probar integridad, restauración, copias de seguridad, exportaciones, cortes de energía y errores de hardware conforme a `../tests/PRODUCTION_ACCEPTANCE.md`. El ejecutable `tpv_maintenance` aporta comprobaciones estructurales de SQLite y pruebas de backup/restore; no acredita la integridad fiscal.
4. Implementar y verificar el módulo fiscal oficial completo, incluida firma XAdES y QR fiscal, antes de emitir facturas reales.
5. Validar el sistema con una persona especializada en facturación/fiscalidad y pruebas reales controladas en Windows.
6. Preparar la declaración responsable del productor para la versión concreta, con la identificación del sistema, versión, composición, funcionalidades y demás contenido exigible. No firmarla ni publicarla hasta poder sostener las afirmaciones de conformidad.
7. Mostrar la declaración responsable de forma visible y conservarla por versión; mantener control de cambios y un procedimiento de actualización.

La declaración responsable es responsabilidad del productor. Este proyecto no está certificado por Hacienda y no debe describirse como homologado.

## Fuentes oficiales

- AEAT — Sistemas informáticos de facturación y Veri*Factu: https://sede.agenciatributaria.gob.es/Sede/iva/sistemas-informaticos-facturacion-verifactu.html
- AEAT — Información técnica, esquemas, validaciones, huella, firma y QR: https://sede.agenciatributaria.gob.es/Sede/iva/sistemas-informaticos-facturacion-verifactu/informacion-tecnica.html
- AEAT — Algoritmo de huella: https://sede.agenciatributaria.gob.es/Sede/iva/sistemas-informaticos-facturacion-verifactu/informacion-tecnica/algoritmo-calculo-codificacion-huella-hash.html
- AEAT — Especificaciones de firma: https://sede.agenciatributaria.gob.es/Sede/iva/sistemas-informaticos-facturacion-verifactu/informacion-tecnica/especificaciones-tecnicas-firma-electronica-registros-evento.html
- BOE — RD 1007/2023: https://www.boe.es/eli/es/rd/2023/12/05/1007
- BOE — Orden HAC/1177/2024: https://www.boe.es/buscar/act.php?id=BOE-A-2024-22138

Consulta estas páginas de nuevo antes de desplegar: las especificaciones y los plazos pueden cambiar.
