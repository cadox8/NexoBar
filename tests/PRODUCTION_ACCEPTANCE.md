# Pruebas de integridad, recuperación, exportación y hardware

**Estado: pruebas de mantenimiento SQLite implementadas; pruebas fiscales y de hardware requieren integración en Windows y siguen pendientes.**

## A. Integridad y recuperación de base de datos

1. Ejecutar `tpv_maintenance check data/tpv.db` y guardar la salida.
2. Ejecutar `tpv_maintenance backup data/tpv.db backups/tpv-YYYYMMDD-HHMM.db`.
3. Verificar la copia con `tpv_maintenance check backups/tpv-YYYYMMDD-HHMM.db`.
4. Restaurar siempre primero a una ruta temporal: `tpv_maintenance restore backups/tpv-YYYYMMDD-HHMM.db data/restore-test.db`.
5. Ejecutar `tpv_maintenance check data/restore-test.db`.
6. Comparar recuentos de ventas, líneas, pagos, artículos y sesiones de caja entre original y restaurada. La comprobación SQLite no demuestra por sí sola que los datos fiscales sean auténticos ni completos.
7. Simular falta de espacio, permisos denegados, archivo inexistente, destino ocupado y fallo de escritura; confirmar que el programa informa del error y no declara una copia correcta.
8. Ensayar recuperación en un equipo de prueba y documentar tiempo de recuperación y pérdida de datos esperada.

### Precauciones

- Mantener copias fuera del disco del TPV y cifrarlas si contienen datos personales.
- Restringir el acceso a las copias.
- La herramienta de restauración crea un archivo de destino diferente; no sustituye automáticamente la base activa.
- Para sustituir la base activa, cerrar primero el TPV, guardar una copia de la base actual y aplicar un procedimiento controlado. No borrar ni editar registros fiscales a mano.
- Una copia SQLite íntegra no sustituye a la firma, el encadenamiento ni la exportación exigidos al SIF.

## B. Exportación

Actualmente no existe exportador fiscal en el prototipo. Antes de producción, cuando se implemente, comprobar:

- Exportación por periodo y por tipo de registro, incluyendo alta, anulación y eventos que correspondan.
- UTF-8 y estructura oficial; no convertir los registros a CSV como único archivo de conservación.
- Recuento de registros exportados frente a la consulta de origen.
- Revisión de que cada registro exportado conserva huella, firma, encadenamiento y referencias.
- Exportación repetida produce los mismos registros lógicos, sin duplicar ni omitir datos.
- Los errores de exportación se registran y no se reporta éxito parcial como éxito completo.

## C. Impresora y cajón — prueba física Windows 11

- Probar con la impresora real y un rollo de prueba; validar UTF-8/codificación, corte, ancho, saltos, importes y alineación.
- Desconectar la impresora, provocar cola atascada y falta de papel si el dispositivo lo permite; el backend debe reportar fallo y no confundir “venta guardada” con “ticket impreso”.
- Verificar apertura del cajón por RJ11/RJ12 conectado a la impresora. No se debe disparar el cajón automáticamente antes de confirmar la operación de cobro.
- Si el ticket falla después del cobro, permitir reimpresión claramente identificada sin crear una segunda venta ni un nuevo registro fiscal.
- La prueba de impresión no es una prueba de conformidad fiscal.

## D. Integridad de registros fiscales (pendiente del módulo fiscal)

- Alterar una copia de prueba de un registro y confirmar que falla la verificación de huella/firma/cadena.
- Restaurar una copia y verificar cadena y firmas completas.
- Verificar registro de evento para arranque/parada, comprobaciones, anomalías, restauraciones y exportaciones según el modo NO VERI*FACTU.
- Comprobar que correcciones/anulaciones añaden los registros exigidos y nunca borran silenciosamente el original.
