# Prueba de latencia de punta a punta (método v2)

Archivos:
- **Receptor:** `Receptor_Latencia/Receptor_Latencia.ino`. Es `Receptor.ino` con agregados marcados `[LATENCIA]`.
- **Transmisor:** `Transmisor/Transmisor.ino`, el firmware normal, sin cambios.
- **Análisis:** `docs/pruebas/matlab/latencia_punta_a_punta.m`.
- **Método anterior (v1)**, guardado como referencia: `docs/pruebas/latencia_v1/`.

## 1. Qué mide

El receptor genera un escalón de tensión que reemplaza al potenciómetro del índice en el guante. Después
mide cuánto tarda en cambiar el pulso PWM que recibe el servo del índice. Los dos instantes se toman con el
reloj del receptor, así que no hace falta sincronizar las placas.

```
escalón en GPIO 34 del Tx ──► espera al ciclo del Tx (0–20 ms) ──► ADC 16× + armado ──► ESP-NOW
   (t0, reloj del Rx)                                                                    │
                     pulso PWM del servo ◄── espera al período PWM (0–20 ms) ◄── tarea ◄─ callback
                     (t1, reloj del Rx, flanco de bajada del primer pulso con el ancho nuevo)
```

| Etapa | v1 (vieja) | v2 (nueva) |
|---|---|---|
| Espera hasta el ciclo siguiente del Tx | No | **Sí** |
| ADC del potenciómetro, cálculo y empaquetado | No | **Sí** |
| ESP-NOW, callback, cola, tarea de control | Sí | Sí |
| Espera hasta el período PWM siguiente y ancho del pulso | No | **Sí** |
| Recorrido completo con el slew-rate | No | **Sí** (columna aparte) |
| Sensor físico (potenciómetro) y movimiento del servo | No | No |
| Camino de la muñeca (IMU, DLPF, filtro complementario) | No | No |

Cada ensayo devuelve tres tiempos:
- `llegada_us`: del escalón al primer paquete que lo trae, validado en el receptor.
- `primer_us`: del escalón al fin del primer pulso PWM con el ancho cambiado. **Es la latencia de punta a
  punta.** Se usa el flanco de bajada porque el servo recién conoce el ancho cuando termina el pulso.
- `final_us`: del escalón al primer pulso con el ancho definitivo. Incluye el limitador de velocidad
  (8° por ciclo) y es lo que tarda la orden completa.

## 2. Qué cambia respecto al código viejo

| | v1 | v2 |
|---|---|---|
| Transmisor | `Transmisor_Latencia.ino` con un pulso en GPIO 2 antes de `esp_now_send()` | **Firmware normal sin tocar**: lo que se mide es el sistema real |
| Inicio | Justo antes del envío por radio | **Cambio en la entrada del sensor** (escalón en GPIO 34) |
| Fin | `micros()` al terminar la tarea, aunque no se hubiera escrito ningún servo | **Fin del primer pulso PWM con el ancho cambiado**, medido en la salida real (GPIO 26) |
| Emparejamiento | Ambiguo: cada pulso pisaba el anterior y los retardos de más de 20 ms quedaban mal | **Inequívoco**: un escalón por vez y no se arranca otro hasta que termina el anterior (o hasta 1 s) |
| Instante del estímulo | Atado al ciclo del Tx | **Aleatorio** (espera de 200–600 ms): cae en cualquier punto del ciclo de 20 ms |
| Salida | `Latencia_ms` al Serial Plotter, con interpolación | Línea CSV por ensayo, con tramos, secuencia, timeouts y pulsos perdidos |
| Estadísticas | Se calculaban y no se imprimían | Comando `s` en el monitor y análisis completo en MATLAB |
| Constantes del receptor | `VEL_MAX_MUNIECA_*` distintas del firmware real | Idénticas a `Receptor.ino` (lo agregado está marcado `[LATENCIA]`) |

Detalles del receptor nuevo:
- **ISR en GPIO 4 (`isrCapturaPwm`).** Toma `micros()` en cada flanco. Si entre dos flancos pasan menos de
  3 ms, es un pulso que terminó: guarda el ancho y el instante.
- **Callback de ESP-NOW.** Después de validar el paquete, si hay un ensayo armado y el ángulo del índice
  cambió 3° o más, guarda el instante de llegada y la secuencia. El resto del callback no cambia.
- **`loop()`.** Antes estaba vacío; ahora corre los ensayos. Tiene prioridad 1, por debajo de la tarea de
  control, y no altera los tiempos porque los instantes los toman la ISR y el callback.
- **Cuándo arranca un ensayo.** Con el enlace activo, el servo quieto (5 pulsos iguales) y la espera
  aleatoria cumplida.
- **El escalón.** Se alterna entre 0 V y 3,3 V. Uno lleva el índice a un extremo (25°) y el otro al
  opuesto (90°), cualquiera sea la calibración guardada.
- **Ensayos que se marcan.** Si `loop()` se salteó algún pulso (`pulsos_perdidos > 0`), el ensayo se marca
  y MATLAB lo descarta. Si pasa 1 s sin terminar, se imprime una línea `T`.

## 3. Materiales

- Las dos placas con su alimentación de siempre. Mano con servos alimentados por el MP1584.
- **Dos computadoras, o una con dos puertos USB.** Solo hace falta leer el puerto del receptor.
- 3 cables dupont macho-hembra (o los que correspondan a tus placas) y una resistencia de 1 kΩ (sirve de
  470 Ω a 4,7 kΩ).
- MATLAB R2020a o posterior.

## 4. Conexiones

| # | Desde | Hasta | Para qué |
|---|---|---|---|
| 0 | — | — | **Desconectar el cursor (pata central) del potenciómetro del índice del GPIO 34 del transmisor.** Si no, el potenciómetro y el GPIO 18 se pelean por la tensión de la línea. |
| 1 | Rx **GPIO 18** | resistencia de 1 kΩ → Tx **GPIO 34** | Escalón que reemplaza al potenciómetro del índice. La resistencia limita la corriente si una placa está apagada. |
| 2 | Rx **GPIO 26** (señal del servo del índice) | Rx **GPIO 4** | La señal PWM real vuelve a entrar para medir su ancho. El servo sigue conectado al GPIO 26. |
| 3 | Rx **GND** | Tx **GND** | Referencia común para el escalón. |

Antes de encender, revisar que el GPIO 34 del transmisor no quede unido al potenciómetro. Los demás dedos y
el IMU quedan como están: no afectan la medición, porque solo se mira el índice.

## 5. Firmware

1. **Transmisor:** cargar `Transmisor/Transmisor.ino` tal cual. Si ya lo tiene, no hace falta cargar nada.
2. **Receptor:** abrir `Receptor_Latencia/Receptor_Latencia.ino` en el Arduino IDE, placa "ESP32 Dev
   Module", y cargarlo. Compila con el core esp32 2.0.17 y ESP32Servo 3.2.1, comprobado el 2026-10-25.
   Es la misma placa receptora, así que la MAC no cambia y el transmisor la sigue encontrando.
3. **Cerrar el monitor serie del Arduino IDE** antes de usar MATLAB: un puerto no se puede abrir dos veces.

## 6. Verificación antes de registrar (2 minutos)

1. Encender los dos. Abrir el monitor serie del receptor a 115200 baudios.
2. Debe aparecer `[LATENCIA] Estímulo en GPIO 18, captura PWM en GPIO 4 (puente desde GPIO 26)` y después
   `[FSM] Enlace establecido`.
3. **El índice de la mano robot debe abrirse y cerrarse solo, cada 0,5–1 s aproximadamente.** Si no se
   mueve:
   - revisar la conexión 1 y el paso 0;
   - las líneas `T,…,0` indican que el paquete no trae el cambio.
4. Deben aparecer líneas como `L,12,1,87,14230,27810,198400,0`. Las líneas `T,…,1` o `T,…,2` indican que
   el GPIO 4 no ve el PWM: revisar el puente 2.
5. Mandar `s`: imprime un resumen con media, mínimo, mediana, p95 y máximo. Mandar `r` para borrarlo antes
   de empezar.
6. Cerrar el monitor serie.

## 7. Registro

1. En MATLAB, pararse en `docs/pruebas/matlab/` y abrir `latencia_punta_a_punta.m`.
2. Ajustar `puertoRx` (el COM del receptor) y dejar `modo = "serial"` y `duracion_s = 420`.
3. Correr el script. Cada 30 s muestra cuántos ensayos lleva; a unos 0,7 s por ensayo son unos 500 en total.
4. **Durante el registro:**
   - No tocar el guante ni la mano.
   - Dejar las placas a la distancia de uso normal y anotarla.
   - Anotar si hay redes Wi-Fi cerca (afectan a ESP-NOW).
5. **Apretar el botón EN del receptor 2 o 3 veces durante el registro, a intervalos de unos 2 minutos.** El
   motivo es la fase entre el período PWM del receptor y el ciclo del transmisor: depende de dos cristales
   distintos y se corre muy despacio. Sin reinicios, los 7 minutos podrían cubrir solo una parte de esa
   espera de 0–20 ms y sesgar el resultado. Cada reinicio arranca el PWM en otra fase. En el gráfico
   `_serie.png` se ve si hubo deriva.
6. Para analizar de nuevo un registro ya guardado: `modo = "archivo"` y `archivoEntrada = "<ruta>_rx.txt"`.

## 8. Qué se obtiene

En `docs/pruebas/evidencia/<fecha>_latencia/`:

| Archivo | Contenido |
|---|---|
| `latencia_<fecha>_rx.txt` | Registro crudo completo, como evidencia |
| `latencia_<fecha>.csv` | Una fila por ensayo: `n, nivel, secuencia, llegada_us, primer_us, final_us, pulsos_perdidos` |
| `_histograma.png` | Distribución de la latencia de punta a punta, con la mediana y el p95 |
| `_tramos.png` | Diagrama de cajas: Tx + enlace, Rx + PWM y punta a punta |
| `_serie.png` | Latencia de cada ensayo en orden: muestra la deriva de fase y los picos |

En la ventana de comandos aparecen:
- la tabla de media, desviación, mínimo, mediana, p95 y máximo por tramo;
- las **filas en LaTeX** listas para pegar;
- la comparación entre los dos sentidos del escalón, que deberían dar parecido.

### Valores esperados para controlar que la prueba tenga sentido

Son una predicción hecha a partir del código, **no resultados**:

| Tramo | Rango esperado |
|---|---|
| Tx + enlace | 0–20 ms de espera + unos 5 ms (ADC, armado, aire) → **unos 5–25 ms** |
| Rx + PWM | 0–20 ms de espera del PWM + 0,8–1,5 ms de pulso → **unos 1–21 ms** |
| Punta a punta | **unos 6–46 ms, con media cerca de 25 ms**. El capítulo 7 estimaba unos 30 ms. |
| Orden final | Punta a punta + unos 8 ciclos de slew-rate → **unos 180–210 ms** |

Si los histogramas de los dos primeros tramos se ven aproximadamente planos (uniformes) en 20 ms, la prueba
se comporta como se espera. Valores muy por fuera de esos rangos son un hallazgo: retransmisiones o
pérdidas de ESP-NOW. Se comparan con `secuencia`: un salto en la secuencia es un paquete perdido.

## 9. Cómo ponerlo en el informe

**Capítulo 7 (Análisis temporal):**
- Cambiar el título de la tabla 7 a "estimación y medición".
- Reemplazar las filas medidas por los valores de la prueba: espera del Tx, ADC y armado, ESP-NOW, tarea del
  receptor y PWM pasan a ser dos filas medidas, "Tx + enlace" y "Rx + PWM", más el total.
- Dejar como estimación solo lo que no se mide: DLPF del IMU (camino de la muñeca) y movimiento mecánico.
- Agregar el diagrama de conexiones (tabla de la sección 4), `_histograma.png` y `_tramos.png`.
- Sacar el `\pendiente` y el párrafo que propone medir.

**Capítulo 9:** sacar el `\pendiente` de latencia, actualizar la limitación ("todavía no se midió") y el
trabajo futuro.

**Presentación:** cambiar la diapositiva de latencia por el histograma y la mediana con el p95.

**Texto sugerido** para la sección 7, completando los valores:

> Para medir la latencia de punta a punta, el receptor genera un escalón de tensión que reemplaza al
> potenciómetro del índice en el guante y mide, en su propia salida, el instante en que termina el primer
> pulso PWM con el ancho nuevo. Como ambos instantes se toman con el reloj del receptor, no se requiere
> sincronizar las placas. El transmisor ejecuta su firmware normal y el receptor sólo agrega la generación
> del escalón y la captura del pulso. El escalón se aplica en instantes aleatorios, de modo que cae en
> cualquier punto del ciclo de 20 ms del transmisor. Sobre N ensayos, la latencia fue de X ms de mediana,
> con un percentil 95 de Y ms y un máximo de Z ms. De ese total, A ms corresponden al transmisor y al enlace,
> y B ms al receptor y a la espera del período PWM. La orden completa, limitada por el control de velocidad
> del receptor, se alcanza en C ms. La medición no incluye el tiempo de respuesta del sensor ni el
> movimiento mecánico del servo, ni el camino de la muñeca, cuyo filtro interno del MPU6050 agrega unos
> 5 ms según la hoja de datos.

### Limitaciones que hay que declarar

- **Lo que queda afuera:**
  - **Sensor físico y movimiento.** Se inyecta un escalón ideal en lugar del potenciómetro y se termina en
    la orden eléctrica al servo, no en su movimiento.
  - **Muñeca.** Se mide el camino de los dedos. La muñeca agrega el DLPF del MPU6050 y el filtro
    complementario, que se mantienen como estimación.
- **Condiciones de la prueba:**
  - **Cables entre las placas.** Durante la prueba hay un GND común entre las placas. No cambia el enlace
    de radio, pero no es la condición de uso.
  - **Ambiente de radio.** Los resultados valen para la distancia y el ambiente del día de la prueba.

## 10. Al terminar

1. Volver a conectar el cursor del potenciómetro del índice al GPIO 34 del transmisor y retirar los
   cables 1, 2 y 3.
2. Cargar de nuevo `Receptor/Receptor.ino` en el receptor.
3. Registrar la prueba en `docs/pruebas/REGISTRO_PRUEBAS.md`: fecha, distancia, ambiente, cantidad de
   ensayos y resultados.
