# Plan de pruebas de laboratorio — alimentación del receptor y validación de la v2.3.0

Pruebas para hacer **en orden, una por una**. Cada una dice qué necesitás, qué hacer, qué guardar
y qué debería pasar. Al terminar cada prueba, completá su cuadrito **"Resultado"**.

**Prioridad:** ★ = imprescindible para el informe · ☆ = si da el tiempo.

---

## Antes de empezar

### Instrumentos
- Multímetro
- Osciloscopio (2 canales como mínimo)
- Pinza amperométrica DC
- Resistencia de medición (*shunt*) de 0,5 Ω y **1 W o más**
- Celular para fotos y video (si graba en cámara lenta, mejor)
- Notebook con Arduino IDE y el monitor serie a **115200 baudios**

### Cómo está alimentado el receptor
| Parte | Alimentación |
|---|---|
| Servos | Fuente de 12 V / 4 A → regulador *buck* MP1584 → 5 V a los pines de la placa base |
| Filtrado | Electrolítico de 470 µF / 35 V + cerámico de 100 nF sobre los 5 V de los servos |
| ESP32 receptor | Por micro-USB desde la notebook. GND unido al de los servos |
| No se usan | Capacitor de 2200 µF, diodo 1N5822 y TVS |

### ⚠️ Regla de seguridad con el osciloscopio
La masa (el cocodrilo) de cada punta está conectada a la **tierra de la red**, y el GND del sistema puede
estar conectado a tierra a través del USB y del cargador de la notebook. Por eso:

- **El cocodrilo va siempre al GND del sistema.** Nunca a +12 V, a +5 V ni a un extremo de una resistencia que no esté en GND.
- Todos los cocodrilos de todas las puntas, al mismo punto de GND.
- Si hay que medir una tensión que no está referida a GND, se usan **dos canales y la resta** (CH1 − CH2), con los dos cocodrilos en GND.

### Dónde guardar las cosas
Creá la carpeta `docs/pruebas/evidencia/<fecha>/` y nombrá cada archivo con el número de la
prueba adelante: `P03_osc_corriente.png`, `P04_osc_caida_5V.png`, `P10_serie_failsafe.txt`, etc.

- **Capturas del osciloscopio:** guardalas por USB si el equipo lo permite. Si no, sacale una foto
  derecha a la pantalla. Anotá siempre la escala (V/div, ms/div).
- **Monitor serie:** cuando una prueba dice "guardá el texto", seleccioná todo en el monitor serie
  (Ctrl+A, Ctrl+C), pegalo en un `.txt` y guardalo.

### Firmware
- Rama `exp/medicion-latencia`. Hoy las dos placas tienen `TELEMETRIA_MATLAB = 1` (salen líneas CSV `T,...` / `R,...`).
- Para la Parte A sirve así. Para las pruebas de texto de la Parte B (P8–P15), conviene volver a `0`
  y reflashear; para P16, dejar en `1`.
- **Abrir o cerrar el monitor serie reinicia la placa** (líneas DTR/RTS del USB). Si la mano va a reposo
  al cerrar el monitor del transmisor, no es una falla.

---

## Parte A — Alimentación

### P1 ★ Verificación del conexionado (sin alimentar los servos)
**Para qué:** confirmar cómo están unidas las alimentaciones antes de medir, y descartar que el regulador
de 5 V y el USB de la notebook se "empujen" entre sí.

1. **Todo desconectado** (sin 12 V y sin USB). Con el multímetro en continuidad:
   - Entre un pin de **5 V de la tira de servos** de la placa base y el pin **5V (o VIN) del ESP32**.
   - Entre un pin **GND de la tira de servos** y el pin **GND del ESP32**.
2. **Sólo USB conectado** (sin 12 V). Medí la tensión:
   - En el pin 5V del ESP32.
   - En un pin de 5 V de la tira de servos.
3. **Sólo 12 V conectado** (sin USB). Fijate si se enciende el LED rojo del ESP32. Medí la tensión en el pin 5V del ESP32.
4. Sacale una foto a la placa del ESP32 de cerca, del lado del conector USB, para ver si tiene un diodo.

| Resultado | |
|---|---|
| 5 V de servos ↔ pin 5V del ESP32 | ☐ unidos ☐ separados |
| GND de servos ↔ GND del ESP32 | ☐ unidos ☐ separados |
| Sólo USB: pin 5V ESP32 / tira de servos (V) | / |
| Sólo 12 V: ¿enciende el ESP32? / pin 5V (V) | / |

**Cómo interpretarlo:**
- **5 V separados:** el ESP32 se alimenta sólo por USB y los servos sólo por el regulador. Es lo que dice el informe. Seguí con P2.
- **5 V unidos:** los dos quedan conectados al mismo punto. Si en el paso 2 la tira de servos marca casi 5 V, el USB está alimentando la línea de los servos; si en el paso 3 el ESP32 enciende, el regulador alimenta al ESP32. **No sigas con la mano en movimiento: avisame** y vemos cómo separarlos (por ejemplo, cortando el cable de 5 V del USB o el puente de la placa base).

---

### P2 ★ Tensión de salida del regulador y consumo medio
**Para qué:** el consumo real del montaje, que reemplaza los valores de las hojas de datos.

1. Alimentá todo (12 V + USB) y encendé el transmisor. Esperá a que la mano siga al guante.
2. Medí con el multímetro la tensión en la tira de 5 V de servos.
3. Con la **pinza DC**, medí la corriente en el **cable de +5 V** que va del regulador a la placa base, y en el **cable de +12 V** de la fuente, en cuatro situaciones:
   - **a.** Mano siguiendo al guante, quieta (mano abierta, muñeca neutra).
   - **b.** Puño cerrado y sostenido (los dedos hacen fuerza).
   - **c.** Moviendo dedos y muñeca en forma continua: anotá el valor más alto que veas.
   - **d.** Transmisor apagado y servos ya desacoplados (unos 2 s después de llegar a reposo).

| Resultado | Tensión 5 V (V) | Corriente 5 V (A) | Corriente 12 V (A) |
|---|---|---|---|
| a. Quieta | | | |
| b. Puño cerrado | | | |
| c. En movimiento (máx.) | | | |
| d. Desacoplados | | | |

**Para tener en cuenta:** la corriente a 12 V es menos de la mitad que a 5 V, porque el regulador
transforma tensión en corriente. Con los dos valores se calcula la eficiencia:
η = (5 V × I₅) / (12 V × I₁₂).

---

### P3 ★ Corriente con el osciloscopio (forma de onda y picos)
**Para qué:** ver los picos de corriente cuando arrancan los servos, que la pinza no alcanza a registrar.

**Conexión (importante, ver la regla de seguridad):**
1. Desconectá la fuente de 12 V.
2. Poné la resistencia de 0,5 Ω en el **cable negativo** de la fuente, entre el **− de la fuente** y el **IN− del regulador**.
3. **Cocodrilo** de la punta en el **IN− del regulador** (ese es el GND del sistema). **Punta** en el lado del **− de la fuente**.
4. El osciloscopio va a mostrar una tensión **negativa**: la corriente es I = −V / 0,5 Ω (por ejemplo, −0,25 V = 0,5 A).

**Configuración del osciloscopio:**
- Acoplamiento DC, 200 mV/div, base de tiempo 20 ms/div.
- **Límite de ancho de banda en 20 MHz** (*BW limit*) y adquisición en **promedio** o *Hi-Res*. Así se ve la corriente que consumen los servos y no los pulsos de conmutación del regulador (que trabaja a cientos de kHz).
- Mediciones automáticas: **Media** (*Mean*) y **Mínimo** (el pico de corriente). El valor RMS **no** representa el consumo.

**Pasos:**
1. Con la mano quieta, guardá una captura y anotá la media: `P03_osc_corriente_reposo.png`.
2. Disparo en flanco **descendente** un poco por debajo del nivel de reposo, modo *Single*. Cerrá el puño de golpe y girá la muñeca. Guardá la captura: `P03_osc_corriente_pico.png`.
3. Repetí el paso 2 tres veces y anotá el pico de cada una.

| Resultado | |
|---|---|
| Corriente media en reposo (A, a 12 V) | |
| Picos de corriente (A, a 12 V) — 3 intentos | / / |
| Duración del pico (ms) | |

---

### P4 ★ Caída de tensión en los 5 V de los servos durante los picos
**Para qué:** comprobar que el regulador y el capacitor de 470 µF sostienen la tensión.

1. Sacá la resistencia de 0,5 Ω y volvé a conectar la fuente normal.
2. **CH1** en la tira de **5 V de servos**; cocodrilo en GND de la tira.
3. Acoplamiento DC, 1 V/div, 10 ms/div, BW limit en 20 MHz. Disparo en flanco **descendente**, unos 0,3 V por debajo de la tensión de P2, modo *Single*.
4. Cerrá el puño de golpe y girá la muñeca. Guardá la captura: `P04_osc_caida_5V.png`.
5. Medí con los cursores la tensión mínima y cuánto dura la caída. Repetí tres veces.
6. ☆ Si el capacitor de 470 µF se puede desconectar fácilmente, repetí sin él (`P04_osc_caida_5V_sin_cap.png`). Así se ve cuánto aporta.

| Resultado | Con 470 µF | ☆ Sin 470 µF |
|---|---|---|
| Tensión en reposo (V) | | |
| Tensión mínima en el pico (V) | | |
| Duración de la caída (ms) | | |

---

### P5 ☆ Ruido en los 5 V de los servos
1. Misma conexión que P4, pero acoplamiento **AC**, 50 mV/div, 2 µs/div, **sin** límite de ancho de banda.
2. Con la mano quieta, guardá la captura (`P05_osc_ripple.png`) y anotá la tensión pico a pico.

| Resultado | |
|---|---|
| *Ripple* pico a pico (mV) | |

---

### P6 ☆ Prueba de esfuerzo
1. Durante **2 minutos**, abrí y cerrá la mano y mové la muñeca en forma brusca y continua.
2. Mirá el monitor serie del receptor: si aparece el mensaje de arranque (`[SISTEMA] Iniciando ...`), se reinició. Anotalo junto con el motivo que imprime.
3. Al terminar, tocá con cuidado el regulador, el capacitor y los servos de la muñeca, y anotá si calientan.

| Resultado | |
|---|---|
| Reinicios del receptor (y motivo) | |
| Temperatura del regulador / capacitor / servos | |

---

### P7 ☆ Uso continuo de la fuente
Se hace junto con P15 (uso continuo de 15 a 30 minutos): cada 5 minutos, anotá la corriente con la pinza y
la temperatura del regulador MP1584. El MP1584 se apaga solo si se calienta demasiado (protección térmica).

---

## Parte B — Funcionamiento con la v2.3.0

El receptor queda conectado por USB, así que su monitor serie está disponible en todas las pruebas.

### P8 ★ Arranque y motivo de reinicio
1. Abrí el monitor serie del receptor y apretá el botón **EN** del ESP32.
2. Guardá el texto del arranque: `P8_serie_arranque_rx.txt`.

**Qué debería pasar:** aparece `[SISTEMA] Motivo del reinicio: encendido, boton EN o reinicio por USB` y después
la mano se sincroniza con el guante. En el ESP32 el botón EN, el corte de alimentación y el reinicio que provoca
el USB al abrir o cerrar el puerto serie dan el mismo motivo; lo que sí se distingue es `BROWNOUT`.

> **Abrir o cerrar el puerto serie reinicia la placa.** El adaptador USB de la placa usa las líneas DTR/RTS
> para manejar el pin EN. Si el monitor serie del **transmisor** se cierra o se reconecta durante una prueba,
> el transmisor se reinicia y la mano entra en *failsafe* (vuelve a reposo). No es una falla de alimentación.

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

### P16 ☆ Respuesta del sistema con MATLAB (figuras para la sección 8)
**Para qué:** graficar la orden del guante y la posición de la mano en el mismo eje de tiempo.

1. En los **dos** firmwares cambiá `#define TELEMETRIA_MATLAB 0` por `1` y flasheá. La telemetría de
   texto de 5 Hz se reemplaza por líneas CSV a 50 Hz (`T,...` en el Tx y `R,...` en el Rx).
2. Abrí `docs/pruebas/matlab/telemetria_tx_rx.m` y ajustá los puertos COM. El monitor serie de Arduino tiene
   que estar **cerrado**, porque dos programas no pueden abrir el mismo puerto.
3. Corré el script y, durante los 20 s de registro, hacé movimientos de muñeca y dedos (lentos y bruscos).
4. Guardá:
   - los `.txt` que genera el script, en la carpeta de evidencia;
   - el gráfico (`P16_matlab_respuesta.png`);
   - lo que imprime en la consola: paquetes procesados y no procesados.
5. **Al terminar, volvé `TELEMETRIA_MATLAB` a `0`** y reflasheá.

**Cómo interpretarlo:** las dos curvas quedan en el reloj del receptor y alineadas por número de paquete,
así que el corrimiento entre ellas es el **retraso del suavizado** (limitador de velocidad), no la latencia
de radio (esa es la P11).

| Resultado | |
|---|---|
| Paquetes no procesados (%) | |
| Retraso visible en un movimiento brusco de muñeca (ms, aprox.) | |

---

## Al terminar la sesión
- [ ] `MEDIR_LATENCIA` y `TELEMETRIA_MATLAB` quedaron en `0` en los dos firmwares.
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
| P16 | 8: respuesta del sistema (figuras) |
| P13, P14 | 8: pruebas con la versión actual (reemplazo de figuras) |
| Todas | 9: conclusiones |
