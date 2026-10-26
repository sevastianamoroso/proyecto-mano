# Registro de pruebas

Una fila por prueba ejecutada. IDs según [`PLAN_PRUEBAS.md`](PLAN_PRUEBAS.md).
IDs P01–P16 según `PLAN_LABORATORIO.md` (rama `exp/medicion-latencia`); evidencia en [`evidencia/`](evidencia/).
Resultado: ✅ cumple · ⚠️ cumple con observaciones · ❌ no cumple.

| Fecha | ID | Versión Tx | Versión Rx | Resultado | Valores medidos / observaciones |
|---|---|---|---|---|---|
| 2026-10-15 | P02 | v2.3.0 | v2.3.0 | ✅ | Salida del regulador 5,14–5,25 V (RMS). Reposo: 75–100 mA medios a 12 V; pulsos de ~0,38 A por *pulse-skipping*, que bajan a ~0,18 A con 470 µF + 100 nF en la entrada del MP1584. |
| 2026-10-15 | P03 | v2.3.0 | v2.3.0 | ⚠️ | Pitch + roll: ~0,21 A medios (ventana de 10 ms), picos de 0,6–0,72 A. Servo bloqueado: ~0,55–0,6 A. El corte de ~0,4 ms al final es el instante en que se soltó el servo. La salida de 5 V con carga casi no varía (sin valores registrados). |
| — | P09 | v2.3.0 | v2.3.0 | ✅ | Pausa de envío durante la calibración verificada en hardware. |
| 2026-10-22 | — | v2.3.0 | v2.3.0 | ✅ | Anular con movimiento dificultoso en GPIO 12 (*strapping*). El mismo servo en GPIO 13 y en GPIO 23 respondió mucho mejor: la falla seguía al pin. Se reasignó `PIN_SERVO_ANULAR` a GPIO 23. |

## Ajustes de calibración encontrados

| Fecha | Parámetro | Valor anterior | Valor nuevo | Motivo |
|---|---|---|---|---|
| | | | | |
