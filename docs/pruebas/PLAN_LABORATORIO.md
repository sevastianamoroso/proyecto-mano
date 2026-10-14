# Plan de pruebas de laboratorio — alimentación del receptor y validación de la v2.3.0

Pruebas para hacer **en orden, una por una**. Cada una dice qué necesitás, qué hacer, qué guardar
y qué debería pasar. Al terminar cada prueba, completá su cuadrito **"Resultado"**.

**Prioridad:** ★ = imprescindible para el informe · ☆ = si da el tiempo.

---

## Antes de empezar

### Instrumentos
- Fuente de laboratorio (para reemplazar o comparar con la fuente de 12 V)
- Multímetro
- Osciloscopio (2 canales como mínimo)
- Pinza amperométrica DC
- Celular para fotos y video (si graba en cámara lenta, mejor)
- Notebook con Arduino IDE y el monitor serie a **115200 baudios**

### Componentes nuevos (receptor)
| Ref. | Componente | Dónde va (según `Receptor/Conexionado_Receptor.drawio`) |
|---|---|---|
| C1 | Electrolítico 2200 µF / 35 V | Entre +5 V y GND del riel de servos, lo más cerca posible de la bornera de servos. Su terminal (−) es el **punto estrella** de masa. |
| D1 | Schottky 1N5822 | En serie entre el +5 V de los servos y el pin **5V** del ESP32. **Banda (cátodo) hacia el ESP32.** |
| C2 | Electrolítico 470 µF / 35 V | Entre el pin 5V del ESP32 (después de D1) y GND. |
| D2 | TVS SMBJ6.0A | **No se monta** (no se consiguió). |

> **Cuidado con la polaridad.** En los electrolíticos, la franja con el signo "−" es el negativo.
> Un electrolítico conectado al revés puede reventar. En el diodo, la banda es el cátodo.

### Dónde guardar las cosas
Creá la carpeta `docs/pruebas/evidencia/2026-10-14/` y nombrá cada archivo con el número de la
prueba adelante: `P03_osc_pico_sin_cap.png`, `P05_foto_montaje.jpg`, `P10_serie_muneca.txt`, etc.

- **Capturas del osciloscopio:** guardalas por USB si el equipo lo permite. Si no, sacale una foto
  derecha a la pantalla.
- **Monitor serie:** cuando una prueba dice "guardá el texto", seleccioná todo en el monitor serie
  (Ctrl+A, Ctrl+C), pegalo en un `.txt` y guardalo. Con ese texto se arman después los gráficos del informe.

### Importante: el USB enmascara los reinicios
Si el receptor está conectado por USB a la notebook, el ESP32 puede alimentarse del USB aunque el
riel de 5 V se caiga, y el *brownout* no aparece. Por eso, en las pruebas de alimentación
(**P3, P6 y P7**) el receptor va **sin USB**. Para saber si se reinició, se mira el **monitor serie
del transmisor**: mientras el receptor reinicia, el transmisor muestra `RF:NO_ACK`.

### Firmware
- Rama `exp/medicion-latencia`: es igual a la v2.3.0, pero el receptor indica al arrancar por qué se reinició
  (`[SISTEMA] Motivo del reinicio: ...`) y ambos tienen el modo de medición de latencia (apagado).
- Flashear transmisor y receptor desde esa rama antes de la prueba P1.

---

## Parte A — Alimentación (antes y después de los componentes nuevos)

### P1 ★ Relevamiento de cómo está alimentada la mano hoy
**Para qué:** saber desde dónde se parte; el informe tiene que describir lo que realmente se armó.

1. Sacale fotos al conexionado actual del receptor, de arriba y de costado.
2. Anotá lo siguiente:
   - Qué fuente se usa (marca, tensión y corriente de la etiqueta). Sacale una foto a la etiqueta.
   - Si hay un regulador *buck* de 5 V (modelo, por ejemplo LM2596 o XL4015) o si los servos toman los 5 V de la placa del ESP32.
   - Por dónde entra la alimentación al ESP32: plug, pin 5V o USB.
   - Si ya hay capacitores. Si hay, anotá su valor.

| Resultado | |
|---|---|
| Fuente | |
| Regulador de 5 V | |
| Entrada del ESP32 | |
| Capacitores existentes | |

---

### P2 ★ Consumo y tensión en reposo, **sin** los componentes nuevos
**Para qué:** tener una referencia para comparar después.

1. Encendé el receptor **sin USB** y el transmisor con USB a la notebook. Esperá a que la mano siga al guante.
2. Con el guante quieto (mano abierta, muñeca en posición neutra):
   - Medí con el multímetro la tensión del riel de servos (+5 V a GND) en la bornera.
   - Medí con la pinza la corriente del cable de +5 V de los servos.
   - Si usás la fuente de laboratorio, anotá la tensión y la corriente que marca.
3. Apagá el transmisor. A los ~2 s la mano va a reposo y los servos se desacoplan. Repetí las mediciones del paso 2.

| Resultado | Mano siguiendo al guante | Servos desacoplados (failsafe) |
|---|---|---|
| Tensión riel 5 V (V) | | |
| Corriente servos (A) | | |
| Fuente: V / A | | |

---

### P3 ★ Picos de corriente y caída de tensión, **sin** los componentes nuevos
**Para qué:** ver cuánto baja la tensión cuando los servos arrancan juntos. Es la figura principal del "antes".

1. Osciloscopio:
   - **CH1** al riel de +5 V de los servos (la masa de la punta al GND del riel).
   - **CH2** al pin **5V** del ESP32 receptor (masa al GND del ESP32).
   - Acoplamiento DC, 1 V/div, base de tiempo 10 ms/div.
   - Disparo (*trigger*) en CH1, flanco **descendente**, unos 0,3 V por debajo de la tensión medida en P2. Modo *Single* o *Normal*.
2. Con el receptor **sin USB**, cerrá el puño de golpe y girá la muñeca al mismo tiempo.
3. Guardá la captura cuando el osciloscopio dispare: `P3_osc_pico_sin_cap.png`.
4. Con los cursores, medí la tensión mínima de cada canal y cuánto dura la caída.
5. Repetí 5 veces el movimiento brusco mirando el monitor serie del **transmisor**. Anotá si aparece `RF:NO_ACK`, porque eso indica que el receptor se reinició.

| Resultado | |
|---|---|
| Tensión mínima CH1 (riel servos) | |
| Tensión mínima CH2 (5V del ESP32) | |
| Duración de la caída (ms) | |
| ¿Hubo reinicios? (cuántos de 5) | |

---

### P4 ★ Montaje de C1, D1 y C2
**Para qué:** dejar documentado el montaje.

1. **Desconectá la alimentación.**
2. Montá C1, D1 y C2 como indica la tabla de "Antes de empezar".
3. Antes de alimentar, revisá con el multímetro:
   - En modo continuidad, que no haya corto entre +5 V y GND.
   - En modo diodo sobre D1, con la punta roja del lado de los servos y la negra del lado del ESP32: tiene que marcar unos 0,2 a 0,4 V. Al revés, no debe conducir.
4. Sacale fotos al montaje terminado, de cerca y con los componentes legibles: `P4_foto_montaje_*.jpg`.

| Resultado | |
|---|---|
| Continuidad +5 V–GND (sin corto) | ☐ OK |
| D1 en modo diodo (V) | |
| Fotos tomadas | ☐ |

---

### P5 ★ Tensiones en reposo con los componentes nuevos
**Para qué:** confirmar que el ESP32 recibe tensión suficiente después del diodo.

1. Alimentá el receptor (sin USB) y encendé el transmisor.
2. Con la mano siguiendo al guante y quieta, medí con el multímetro:
   - La entrada del regulador (12 V).
   - El riel de servos (+5 V), antes de D1.
   - El pin 5V del ESP32, después de D1.
   - El pin 3V3 del ESP32.
3. Calculá la caída en el diodo: riel de servos menos el pin 5V.
4. Repetí las mediciones de corriente de P2, con la mano siguiendo al guante y con los servos desacoplados.

| Resultado | |
|---|---|
| Entrada 12 V (V) | |
| Riel servos (V) | |
| Pin 5V del ESP32 (V) | |
| Caída en D1 (V) | |
| Pin 3V3 (V) | |
| Corriente servos: siguiendo / desacoplados (A) | / |

**Qué debería pasar:** el pin 3V3 marca unos 3,3 V. Según el diagrama, el pin 5V del ESP32 debería
estar cerca de 4,6 V.

> **Ojo con este punto.** Según su hoja de datos, el regulador de 3,3 V de la placa (AMS1117) necesita
> en el peor caso hasta 1,3 V más en la entrada, o sea unos 4,6 V; con menos corriente necesita menos.
> Si el pin 3V3 marca claramente menos de 3,3 V (por ejemplo, 3,1 V), el diodo deja al ESP32 sin margen:
> anotalo y avisame antes de seguir. Una opción es subir la salida del *buck* a unos 5,2–5,3 V,
> sin pasar los 6 V que admiten los servos.

---

### P6 ★ Picos de corriente y caída de tensión, **con** los componentes nuevos
**Para qué:** es el "después" de P3. Comparando las dos capturas se ve si los componentes sirven.

1. Repetí **exactamente** P3, con la misma configuración del osciloscopio y el mismo movimiento.
2. Guardá la captura: `P6_osc_pico_con_cap.png`.
3. Anotá los mismos valores que en P3.

| Resultado | Sin componentes (P3) | Con componentes (P6) |
|---|---|---|
| Tensión mínima CH1 (riel servos) | | |
| Tensión mínima CH2 (5V del ESP32) | | |
| Duración de la caída (ms) | | |
| Reinicios (de 5) | | |

**Qué debería pasar:** CH2 (ESP32) baja menos que CH1, porque D1 aísla al ESP32 y C2 lo sostiene, y no hay reinicios.

---

### P7 ☆ Prueba de esfuerzo de la alimentación
1. Con el receptor sin USB, durante **2 minutos** abrí y cerrá la mano y mové la muñeca en forma brusca y continua.
2. Mirá el monitor serie del transmisor y contá las veces que aparece `RF:NO_ACK`.
3. Al terminar, tocá con cuidado el regulador, C1 y los servos de la muñeca, y anotá si calientan. Si tenés un termómetro, medí la temperatura.

| Resultado | |
|---|---|
| Veces que apareció NO_ACK | |
| Temperatura del regulador / C1 / servos de muñeca | |

---

## Parte B — Funcionamiento con la v2.3.0

Desde acá podés conectar el receptor por USB para ver su monitor serie.

### P8 ★ Arranque y motivo de reinicio
1. Abrí el monitor serie del receptor y apretá el botón **EN** del ESP32.
2. Guardá el texto del arranque: `P8_serie_arranque_rx.txt`.

**Qué debería pasar:** aparece `[SISTEMA] Motivo del reinicio: boton EN / externo` y después la mano se sincroniza con el guante.

---

### P9 ★ Calibración de los dedos con pausa de envío (pendiente del informe, sección 4)
**Para qué:** validar en hardware algo que se agregó en la v2.3.0 y nunca se probó.

1. En el monitor serie del **transmisor**, mandá `c`.
2. Comprobá que el transmisor deja de imprimir la telemetría `[TX #...]`.
3. Comprobá que en el receptor aparece `[ALERTA FAILSAFE]` y que la mano va suave a la posición de reposo.
4. Seguí los pasos: mano abierta + ENTER, puño cerrado + ENTER.
5. Al terminar, comprobá que el transmisor vuelve a mandar y que la mano sigue al guante **sin reiniciar nada**.
6. Guardá el texto de ambos monitores: `P9_serie_calibracion_tx.txt` y `P9_serie_calibracion_rx.txt`.
7. Filmá la mano durante toda la calibración: `P9_video_calibracion.mp4`.

| Resultado | |
|---|---|
| Se pausó la telemetría del Tx | ☐ |
| La mano fue a reposo durante la calibración | ☐ |
| Volvió a seguir al guante sola | ☐ |

---

### P10 ★ Failsafe al apagar el transmisor
1. Osciloscopio **CH1** a la señal de un servo de dedo en el receptor (por ejemplo, GPIO25, el pulgar), 5 ms/div.
2. Con la mano siguiendo al guante, **desenchufá el transmisor**.
3. Medí con un cronómetro, o mejor en un video, cuánto tarda la mano en empezar a ir a reposo.
4. Comprobá en el osciloscopio que, unos 2 s después de llegar a reposo, la señal PWM **desaparece** (desacople). Guardá la captura: `P10_osc_pwm_detach.png`.
5. Volvé a enchufar el transmisor y comprobá que la mano se resincroniza.
6. Guardá el texto del receptor: `P10_serie_failsafe.txt`.

| Resultado | |
|---|---|
| Tiempo hasta que reacciona (aprox.) | |
| La señal PWM se corta después de llegar a reposo | ☐ |
| Se resincronizó al volver el Tx | ☐ |

---

### P11 ★ Latencia de punta a punta con el osciloscopio
**Para qué:** reemplazar la estimación de la sección 7 por una medición real.

**Cómo funciona:** con el modo de medición activo, cada placa cambia el estado de su **GPIO23** en
cada paquete: el transmisor al empezar a leer los sensores y el receptor al terminar de procesar ese
mismo paquete. En el osciloscopio se ven dos ondas cuadradas iguales. El corrimiento entre ellas es la latencia.

1. En **ambos** firmwares (rama `exp/medicion-latencia`), cambiá `#define MEDIR_LATENCIA 0` por `1` y flasheá.
   Al arrancar, cada placa imprime `[LATENCIA] Modo medición activo`.
2. Osciloscopio:
   - **CH1** al GPIO23 del **transmisor**.
   - **CH2** al GPIO23 del **receptor**.
   - Las masas de las dos puntas, cada una al GND de su placa.
   - 2 V/div, 5 ms/div, disparo en CH1 por flanco ascendente.
3. Las dos señales son cuadradas y cada nivel dura unos 20 ms. Medí con los cursores el tiempo entre
   el flanco de CH1 y el **siguiente** flanco de CH2. Esa es la latencia.
4. Tomá **10 mediciones** en distintos momentos, o usá la función de medición *Delay* si el
   osciloscopio la tiene, y anotá el mínimo, el máximo y el promedio.
5. Guardá una captura representativa: `P11_osc_latencia.png`.
6. ☆ Si tenés un tercer canal, conectalo a la señal de un servo (GPIO25 del receptor) para ver cuándo
   sale el pulso PWM nuevo después del flanco de CH2.
7. **Al terminar, volvé `MEDIR_LATENCIA` a `0`** en los dos firmwares y reflasheá.

| Resultado | |
|---|---|
| Latencia mínima (ms) | |
| Latencia máxima (ms) | |
| Latencia promedio (ms) | |
| Mediciones tomadas | |

**Qué debería pasar:** sumando las etapas de la tabla de la sección 7 (lectura de sensores, envío y
procesamiento), se esperan unos 5 a 15 ms, sin contar la espera hasta el próximo pulso PWM. Si la medición
diera más de 20 ms, avisame, porque el método de "siguiente flanco" deja de ser válido. Si se pierde un
paquete, en CH2 falta un flanco: descartá esa medición.

---

### P12 ☆ Latencia mecánica con video en cámara lenta
**Para qué:** medir el tiempo total, incluido el movimiento del servo, como lo percibe el operador.

1. Poné el guante y la mano robótica uno al lado del otro, dentro del mismo cuadro de la cámara.
2. Filmá en cámara lenta (120 o 240 fps) mientras cerrás **un dedo de golpe**. Repetí 5 veces.
3. Guardá el video: `P12_video_camara_lenta.mp4`. Los cuadros se cuentan después: a 240 fps, cada cuadro dura ≈ 4,2 ms.

---

### P13 ★ Muñeca: deriva en reposo y retorno a la posición
**Para qué:** son las nuevas figuras de la sección 8, que hoy muestra mediciones de la versión preliminar.

1. **Deriva:** dejá el guante quieto sobre la mesa **5 minutos** con el monitor serie del transmisor
   abierto. Guardá el texto: `P13_serie_deriva.txt`.
2. **Retorno:** partí de la mano horizontal, inclinala unos 30° hacia arriba, sostené 5 s y volvé a la
   horizontal. Repetí **10 veces**. Guardá el texto: `P13_serie_retorno.txt`.
3. Filmá la mano robótica durante el paso 2: `P13_video_retorno.mp4`.

| Resultado | |
|---|---|
| Variación de V y R en 5 min (°) | |
| ¿Vuelve siempre al mismo valor en la horizontal? | |

---

### P14 ★ Dedos: recorrido completo
1. Abrí y cerrá la mano 10 veces, despacio y después rápido.
2. Comprobá que cada dedo de la mano robótica llega a abrir y cerrar del todo y que en reposo **no zumba**.
3. Guardá el texto del receptor (`P14_serie_dedos.txt`) y un video corto (`P14_video_dedos.mp4`).

| Dedo | Recorrido completo | Zumbido en reposo | Observaciones |
|---|---|---|---|
| Pulgar | | | |
| Índice | | | |
| Medio | | | |
| Anular | | | |
| Meñique | | | |

---

### P15 ☆ Uso continuo (15 a 30 minutos)
1. Usá el sistema en forma continua y normal.
2. Cada 5 minutos, medí la corriente con la pinza y tocá el regulador y los servos.
3. Anotá cualquier reinicio, comportamiento raro o calentamiento.

| Minuto | Corriente (A) | Temperatura / observaciones |
|---|---|---|
| 0 | | |
| 5 | | |
| 10 | | |
| 15 | | |

---

## Al terminar la sesión
- [ ] `MEDIR_LATENCIA` quedó en `0` en los dos firmwares.
- [ ] Todos los archivos están en `docs/pruebas/evidencia/2026-10-14/`.
- [ ] Este plan tiene los cuadros de resultado completos.
- [ ] Commit y push (o pasame la carpeta y lo hago yo).

## Qué parte del informe completa cada prueba
| Prueba | Sección del informe |
|---|---|
| P1–P7 | 5.2 Conexionado y alimentación: pendiente "en hardware" y consumo medido |
| P9 | 4: pendiente de la pausa durante la calibración |
| P10 | 5.3: máquina de estados y failsafe (validación) |
| P11, P12 | 7: análisis temporal, latencia medida |
| P13, P14 | 8: pruebas con la versión actual (reemplazo de figuras) |
| Todas | 9: conclusiones |
