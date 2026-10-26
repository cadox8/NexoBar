# Plan de pruebas de conformidad técnica AEAT — NO VERI*FACTU

**Estado: BLOQUEADO / NO IMPLEMENTADO.** El backend actual no contiene todavía el módulo fiscal, el generador XML fiscal, el cálculo oficial de huellas, la firma XAdES, el registro de eventos fiscal ni el QR de factura. Por tanto, estas pruebas no se pueden marcar como superadas ejecutando el prototipo actual.

Este documento operacionaliza el paso 2: reproducir las especificaciones y ejemplos oficiales publicados por la AEAT. No reemplaza los documentos oficiales ni crea vectores esperados propios.

## Fuentes de referencia

- Portal técnico AEAT: https://sede.agenciatributaria.gob.es/Sede/iva/sistemas-informaticos-facturacion-verifactu/informacion-tecnica.html
- Diseños de registro, esquemas, validaciones y errores: disponibles desde el portal técnico anterior.
- Algoritmo de huella: https://sede.agenciatributaria.gob.es/Sede/iva/sistemas-informaticos-facturacion-verifactu/informacion-tecnica/algoritmo-calculo-codificacion-huella-hash.html
- Firma electrónica de registros: https://sede.agenciatributaria.gob.es/Sede/iva/sistemas-informaticos-facturacion-verifactu/informacion-tecnica/especificaciones-tecnicas-firma-electronica-registros-evento.html
- Orden HAC/1177/2024: https://www.boe.es/buscar/act.php?id=BOE-A-2024-22138

Antes de ejecutar las pruebas, descarga los ejemplos oficiales vigentes y conserva una copia con fecha/versión en `tests/fixtures/aeat/`. No edites manualmente las salidas esperadas de la AEAT.

## Casos obligatorios al implementar `FiscalService`

| ID | Prueba | Resultado esperado | Estado actual |
|---|---|---|---|
| AEAT-01 | Serialización de un registro de alta | XML UTF-8 conforme al XSD oficial; valores/campos y orden exactos | BLOQUEADO: falta FiscalService |
| AEAT-02 | Registro de anulación | XML conforme al diseño oficial y referencia al registro correspondiente | BLOQUEADO |
| AEAT-03 | Huella del alta con vector oficial | Coincidencia exacta con el valor oficial | BLOQUEADO |
| AEAT-04 | Huella de anulación con vector oficial | Coincidencia exacta con el valor oficial | BLOQUEADO |
| AEAT-05 | Encadenamiento de registros | El hash previo coincide y la secuencia es comprobable hacia delante/atrás | BLOQUEADO |
| AEAT-06 | Firma XAdES Enveloped Signature | Validación criptográfica positiva con certificado de prueba admitido | BLOQUEADO |
| AEAT-07 | Firma alterada | La comprobación debe fallar tras alterar un byte firmado | BLOQUEADO |
| AEAT-08 | Registro de evento y su huella/firma | Coincide con el diseño y especificación oficial | BLOQUEADO |
| AEAT-09 | QR fiscal | URL y datos exactos según la especificación técnica vigente | BLOQUEADO |
| AEAT-10 | Rechazo de XML inválido | El validador lo rechaza con error identificable | BLOQUEADO |
| AEAT-11 | Prueba de fechas, zona horaria y cambios de día | Formato y secuencia temporal conforme a la especificación | BLOQUEADO |
| AEAT-12 | Exportación de registros de un periodo | Todos los registros requeridos, legibles, verificables y sin omisiones | BLOQUEADO |

## Cómo registrar resultados

Para cada prueba guarda: identificador, versión del software, versión/fecha de la especificación AEAT, fixture de entrada, salida generada, salida esperada, herramienta usada para comparar, resultado y logs. Los certificados y claves de prueba no deben ser los certificados ni las claves de producción.

No se considerará aprobada una prueba por comparar solo el hash de un JSON inventado, ni por obtener una firma válida sobre un formato distinto al exigido. Debe probarse el XML/canonización/campos exactos definidos por AEAT.
