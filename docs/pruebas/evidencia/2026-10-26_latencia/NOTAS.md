# P11 – Latencia de punta a punta (2026-10-26)

Método v2 (`docs/pruebas/PRUEBA_LATENCIA.md`). Ensayos hechos por Joaquín Petruf. Tx con `Transmisor.ino` v2.3.0.

| Archivo | Contenido |
|---|---|
| `P11_ensayos_976-1003.txt` | Monitor serie con `Receptor_Latencia.ino` del repo (commit `24db766`): 28 líneas `L` y 3 `[RESUMEN]`. Ensayos individuales. |
| `Receptor_Latencia_plotter_littlefs.ino` | Variante hecha por Joaquín: en modo plotter imprime y guarda en LittleFS sólo la media de cada bloque de 15 ensayos. |
| `P11_medias_bloques_15.txt` | Volcado de `/mediciones.txt` de esa variante: 28 medias de bloques de 15 ensayos (≈ 420 ensayos). |
| `P11_serial_plotter_medias.jpeg` | Serial Plotter de esa corrida (eje X: número de bloque; eje Y: media del bloque en ms). |

## Resultados (campo `primer_us`: escalón → fin del primer pulso PWM cambiado)

- Ensayos individuales (n = 28): media 31,4 ms, desvío 6,0 ms, mín. 22,8 ms, máx. 42,3 ms. Sin timeouts ni pulsos perdidos.
  La llegada del paquete al Rx (`llegada_us`) promedia 15,7 ms.
- Medias de bloques de 15 (28 bloques, ≈ 420 ensayos): media general 23,9 ms; medias de bloque entre 20,1 y 35,0 ms
  (26 de 28 entre 20 y 26 ms).

## Limitaciones

- La variante de bloques no guarda los ensayos individuales: no hay mínimo, máximo, p95 ni timeouts de esa corrida.
- No consta si el archivo de LittleFS se borró (`b`) antes de empezar ni cuándo se apretó EN. Los dos bloques de
  33,6 y 35,0 ms podrían corresponder a otra fase entre placas o a otra corrida.
- Los relojes del Tx y del Rx casi no se desfasan entre sí, así que la espera al pulso PWM queda casi fija durante
  una corrida. Eso explica la diferencia entre las dos corridas (31,4 frente a 23,9 ms).
