# Evidencia 2026-10-15 — Osciloscopio Teledyne LeCroy

Reloj del osciloscopio desfasado (marca 12/02/2002): la fecha real es 2026-10-15.

**Conexión:** C1 = shunt de 0,5 Ω en el negativo de 12 V (punta x1, cocodrilo en IN− del MP1584) → I₁₂ = −2·V_C1.
C2 no se usa en ninguna captura (marcaba ~0,5 V por una configuración de punta incorrecta al inicio; se ignora).

**Firmware:** todas las pruebas se hicieron con la v2.3.0, con resultado correcto.

**Filtrado del regulador:** 470 µF / 35 V electrolítico + 100 nF cerámico en la **entrada** (IN+/IN−) y otro par igual en la **salida** (5 V de servos, junto a la placa base).

**Ojo con la tabla de mediciones:** las filas mean/min/max son estadísticas del parámetro (ampl, rms) y vienen
acumuladas de capturas anteriores (no se hizo *Clear Sweeps*). No hay `mean(C1)`: el consumo medio no está medido.

| Archivo | Original | Situación | Escala C1 | Base de tiempo |
|---|---|---|---|---|
| `P02_mov_1Vdiv.png` | LeCroy1 | Movimiento de la mano | 1 V/div | 100 ms/div |
| `P02_mov_200mVdiv.png` | LeCroy2 | Movimiento de la mano | 200 mV/div | 20 ms/div |
| `P02_reposo.png` | LeCroy3 | Reposo (pulsos cada ~1,2–1,5 ms, hipótesis: *pulse-skipping* del MP1584) | 200 mV/div | 1 ms/div |
| `P03_servo_bloqueado.png` | LeCroy4 | Servo bloqueado a mano (≈0,55–0,6 A a 12 V); corte a cero de ~0,4 ms al final, sin explicar | 200 mV/div | 1 ms/div |
| `P02_V5_regulador.png` | LeCroy5 | Salida del regulador, C1 solo: 5,14–5,25 V (RMS) | 1 V/div | 10 ms/div |

## Pulsos en reposo (C1, ~200 mV ≈ 0,4 A cada ~1 ms)
- El período varía levemente alrededor de 1 ms (no es un reloj digital fijo).
- Al desconectar los servos, los pulsos se espacian más; con más carga (servo bloqueado) la conmutación es continua.
- Conclusión: modo ráfaga (*pulse-skipping*) del MP1584 con poca carga. Funcionamiento normal, no es falla.
- Pendiente para cerrar: zoom de un pulso (1 µs/div, sin BW limit) mostrando la conmutación interna.

## Capacitor en la entrada del MP1584 (C1 invertido, 100 mV/div → I₁₂ = 2·V_C1)
Capacitores de entrada: 470 µF + 100 nF entre IN+ e IN− (las capturas "sin capacitor" son sin este par; el par de salida estuvo siempre).

| Archivo | Original | Situación | Pico C1 → I₁₂ | mean(C1) → I₁₂ | Base de tiempo |
|---|---|---|---|---|---|
| `P02_reposo_sin_cap_entrada.png` | LeCroy6 | Reposo, sin capacitor | ~190 mV → ~0,38 A (pulsos de ~0,3 ms cada ~0,8 ms) | 51,5 mV → ~0,10 A (prom. estadístico 36 mV → 73 mA) | 1 ms/div |
| `P02_reposo_con_cap_entrada.png` | LeCroy8 | Reposo, con capacitor | 88,6 mV → ~0,18 A (diente de sierra) | 38 mV → ~76 mA | 500 µs/div |
| `P02_reposo_con_cap_entrada_2.png` | LeCroy9 | Igual a LeCroy8: mismas mediciones (equipo en Stop, misma adquisición) | — | — | 500 µs/div |
| `P03_pitch_roll_con_cap.png` | LeCroy10 | Pitch y roll simultáneos, con capacitor | 304 mV → ~0,6 A (máx. estadístico 359 mV → ~0,72 A) | 106 mV → ~0,21 A en la ventana de 10 ms | 1 ms/div |

- El capacitor reduce el pico de corriente de entrada en reposo a la mitad (~0,38 → ~0,18 A) sin cambiar la media (~75–100 mA): cambia la forma, no el consumo.
- Parte del filtrado lo hace el propio shunt (0,5 Ω × 470 µF ≈ 0,24 ms, del orden del período de ráfaga). Sin shunt el suavizado visto por la fuente sería menor.

## P16 — Respuesta del sistema con el graficador MATLAB (v2.3.0, `TELEMETRIA_MATLAB = 1`)
Funcionamiento del graficador: [`EXPLICACION_GRAFICADOR_MATLAB.md`](../../EXPLICACION_GRAFICADOR_MATLAB.md).
Arriba, la orden del Tx ubicada en el instante del reloj del Rx en que se procesó esa trama (alineada por `secuencia`); abajo, la posición aplicada por el Rx.

| Archivo | Original | Señal |
|---|---|---|
| `P16_matlab_pulgar.jpg` | IMG-20261015-WA0018 | Pulgar: tres flexiones (25° ↔ 90°) |
| `P16_matlab_indice.jpg` | IMG-20261015-WA0019 | Índice: flexiones completas y parciales (~57°, ~46°) |
| `P16_matlab_muneca_rotacional.jpg` | IMG-20261015-WA0020 | Muñeca rotacional: 91° → 18° → 139° → 87° |
| `P16_matlab_muneca_vertical.jpg` | IMG-20261015-WA0021 | Muñeca vertical: 59° → 32° → 90° → 45° → 73° → 57° |

- **Eje de tiempo:** dice "ms", pero el factor de escala ×10⁵ de MATLAB quedó fuera de la captura (cada marca = 500 ms; ventana de 5 s).
- Los pequeños saltos de ±1° de la orden en las mesetas no aparecen en el Rx: es la banda muerta (1° dedos, 1,2° muñeca). Por eso también el Rx puede quedar 1° corrido del Tx (índice en reposo: 25° Tx, 26° Rx).
