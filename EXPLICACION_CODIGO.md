# Explicación Detallada del Firmware de Telerrobótica

**Proyecto:** Movilidad de Mano Impresa en 3D en Base al Sensado de los Movimientos (Telerrobótica)
**Institución:** Departamento de Ingeniería Eléctrica y Computadoras (DIEC) – Universidad Nacional del Sur (UNS)
**Autores:** Joaquín Petruf, Sebastián Amoroso
**Versión de firmware documentada:** `v2.3.0` (`v2.2.0` validada en hardware + pausa de transmisión durante la calibración)

> Este documento recorre el código parte por parte: qué hace cada módulo, por qué está hecho así y de
> dónde sale cada técnica. La versión gráfica y resumida está en [`docs/DIAGRAMAS.md`](docs/DIAGRAMAS.md).
> La sección [8. Evolución del diseño](#8-evolución-del-diseño) explica qué se había planteado en las
> versiones anteriores (filtro de Kalman, limitadores lentos, etc.) y por qué se cambió.

---

## Índice

1. [Visión general](#1-visión-general)
2. [Archivos, librerías y hardware](#2-archivos-librerías-y-hardware)
3. [Protocolo de comunicación (`ProtocoloBrazo.h`)](#3-protocolo-de-comunicación-protocolobrazoh)
4. [Transmisor (`Transmisor.ino`)](#4-transmisor-transmisorino)
5. [Receptor (`Receptor.ino`)](#5-receptor-receptorino)
6. [Cadena temporal de un movimiento](#6-cadena-temporal-de-un-movimiento)
7. [Guía de ajuste de parámetros](#7-guía-de-ajuste-de-parámetros)
8. [Evolución del diseño](#8-evolución-del-diseño)
9. [Observaciones conocidas y mejoras futuras](#9-observaciones-conocidas-y-mejoras-futuras)
10. [Referencias](#10-referencias)

---

## 1. Visión general

El sistema replica los movimientos de la mano y la muñeca de un operador sobre una mano robótica impresa
en 3D. Está formado por dos nodos ESP32 que se comunican sin router mediante **ESP-NOW**:

| Nodo | Sensores / actuadores | Responsabilidad |
|---|---|---|
| **Transmisor** (guante) | 5 potenciómetros rotativos (uno por dedo) + IMU MPU6050 (muñeca) | Medir, convertir a ángulos de servo y enviar 50 paquetes por segundo |
| **Receptor** (mano) | 5 servos SG90 (dedos) + 2 servos MG946R (muñeca vertical y rotación) | Validar los paquetes, suavizar la trayectoria, proteger la mecánica y mover los servos |

Una decisión de diseño atraviesa todo el código: **el transmisor envía ángulos de servo ya calculados**
(25–90° para dedos, etc.), no lecturas crudas. Así toda la calibración del operador vive en el guante y
el receptor sólo se ocupa de mover motores de forma segura. Si se cambia de operador, se recalibra el
guante; la mano no se toca.

---

## 2. Archivos, librerías y hardware

### 2.1. Archivos del repositorio

| Archivo | Rol |
|---|---|
| `Transmisor/Transmisor.ino` | Firmware del guante |
| `Receptor/Receptor.ino` | Firmware de la mano |
| `Transmisor/ProtocoloBrazo.h` y `Receptor/ProtocoloBrazo.h` | Definición del paquete y del checksum. **Son dos copias idénticas** (el IDE de Arduino sólo compila archivos de la carpeta del sketch) y deben mantenerse iguales |
| `Transmisor/KalmanFilter.h` | Filtro de Kalman de la `v1.0.0`. **Ya no se usa** desde la `v2.0.0`; se conserva como referencia histórica |
| `*/Conexionado_*.drawio` | Diagramas de conexionado eléctrico |

### 2.2. Librerías

| Librería | Dónde | Para qué |
|---|---|---|
| `WiFi.h`, `esp_now.h` (núcleo Arduino-ESP32) | Ambos | Radio y protocolo ESP-NOW |
| `esp_idf_version.h` | Ambos | Elegir la firma correcta de los callbacks de ESP-NOW según la versión de ESP-IDF (cambió en la 5.0) |
| FreeRTOS (incluido en el núcleo) | Ambos | Tareas, temporización determinista y colas |
| `Adafruit_MPU6050` + `Adafruit_Sensor` | Transmisor | Lectura del IMU por I2C; entrega aceleración en m/s² y giro en rad/s |
| `Wire.h` | Transmisor | Bus I2C |
| `Preferences.h` | Transmisor | Guardar la calibración de los dedos en la memoria flash no volátil (NVS) |
| `ESP32Servo` | Receptor | Generar la señal PWM de los servos con el periférico LEDC del ESP32 |

### 2.3. Asignación de pines

**Transmisor**

| Señal | GPIO | Comentario |
|---|---|---|
| Pulgar, Índice, Medio, Anular, Meñique | 39, 34, 35, 32, 33 | Todos del **ADC1**. El ADC2 del ESP32 no puede usarse mientras la radio Wi-Fi está activa, por eso se eligieron estos pines |
| I2C SDA / SCL (MPU6050) | 21 / 22 | Pines I2C por defecto del ESP32, bus a 400 kHz |

**Receptor**

| Servo | GPIO |
|---|---|
| Pulgar, Índice, Medio, Anular, Meñique | 25, 26, 27, 12, 13 |
| Muñeca vertical / Muñeca rotacional | 32 / 33 |

> Nota de hardware: el GPIO 12 es un pin de *strapping* del ESP32 (define la tensión de la flash al
> arrancar). Si el servo lo mantuviera en alto durante el reset la placa podría no arrancar. En las pruebas
> no causó problemas, pero conviene tenerlo presente si aparece un fallo de arranque.

---

## 3. Protocolo de comunicación (`ProtocoloBrazo.h`)

### 3.1. Estructura del paquete

```cpp
#pragma pack(push, 1)
struct MensajeBrazo {
    uint16_t magicHeader;        // 0xAA55
    uint8_t  secuencia;          // 0..255, incremental
    uint16_t anguloDedos[5];     // 25..90°
    int16_t  muniecaVertical;    // 25..90°
    int16_t  muniecaRotacional;  // 0..180°
    uint16_t checksum;           // Fletcher-16
};
#pragma pack(pop)
```

| Byte | Campo | Tamaño | Contenido |
|---|---|---|---|
| 0–1 | `magicHeader` | 2 | Firma fija `0xAA55` |
| 2 | `secuencia` | 1 | Contador que da la vuelta en 255 |
| 3–12 | `anguloDedos[0..4]` | 10 | Pulgar, índice, medio, anular, meñique |
| 13–14 | `muniecaVertical` | 2 | Ángulo del servo de muñeca vertical |
| 15–16 | `muniecaRotacional` | 2 | Ángulo del servo de rotación |
| 17–18 | `checksum` | 2 | Fletcher-16 de los bytes 0–16 |
| | **Total** | **19 bytes** | Muy por debajo del máximo de 250 bytes de ESP-NOW |

**`#pragma pack(push, 1)`**: el compilador normalmente agrega bytes de relleno (*padding*) para alinear
los campos de 16 bits a direcciones pares. Con `pack(1)` el struct ocupa exactamente 19 bytes y su
disposición en memoria es la misma en los dos ESP32, lo que permite enviarlo tal cual
(`reinterpret_cast<uint8_t*>`) y reinterpretarlo del otro lado sin serializar campo por campo.

**`enum DedoIndex`**: da nombre a los índices 0–4 y define `TOTAL_DEDOS = 5`, que se usa como tamaño de
todos los arreglos por dedo en ambos firmwares.

### 3.2. Checksum Fletcher-16

```cpp
for (size_t i = 0; i < longitud; ++i) {
    sum1 = (sum1 + buffer[i]) % 255;
    sum2 = (sum2 + sum1) % 255;
}
return (sum2 << 8) | sum1;
```

- **De dónde sale:** es el checksum aritmético propuesto por J. G. Fletcher (1982) [1]. Usa dos sumas
  módulo 255: `sum1` es la suma de los bytes y `sum2` la suma acumulada de `sum1`.
- **Por qué dos sumas:** una suma simple no detecta si dos bytes llegan intercambiados. `sum2` depende de
  la **posición** de cada byte, por lo que sí lo detecta. Detecta además todos los errores de un solo bit.
- **Por qué no CRC:** un CRC-16 es algo más robusto, pero Fletcher se calcula con sumas, sin tablas ni
  polinomios, y para 17 bytes el costo es despreciable. La capa MAC de Wi-Fi ya tiene su propio CRC-32,
  así que este checksum es una segunda barrera a nivel de aplicación (por ejemplo, contra un paquete de
  otro dispositivo ESP-NOW con el mismo tamaño).

`validarPaqueteBrazo()` acepta un paquete sólo si la firma es `0xAA55` **y** el checksum coincide.

---

## 4. Transmisor (`Transmisor.ino`)

### 4.1. Bloque de configuración

Todas las constantes ajustables están al principio del archivo:

| Constante | Valor | Significado |
|---|---|---|
| `PERIODO_CONTROL_MS` | 20 | Período del lazo: 50 Hz |
| `ALPHA_COMPLEMENTARIO` | 0.98 | Peso del giróscopo en el filtro de la muñeca (ver 4.7) |
| `TRIM_PITCH_DEG`, `TRIM_ROLL_DEG` | 0 | Corrección del montaje del MPU en el guante: el ángulo que se lee con la mano en pose neutra |
| `PITCH_MANO_ARRIBA` / `PITCH_MANO_ABAJO` | 45° / −25° | Rango útil de flexión de la muñeca del operador |
| `SERVO_V_MIN` / `SERVO_V_NEUTRO` / `SERVO_V_MAX` | 25 / 57 / 90° | Rango mecánico del servo de muñeca vertical |
| `ROLL_MANO_MAX` | 90° | Rotación de la mano: ±90° → 0..180° de servo |
| `macReceptor[]` | `1C:C3:AB:D2:14:9C` | Dirección MAC del ESP32 receptor (la imprime el receptor al arrancar) |

### 4.2. Secuencia de arranque (`setup`)

1. **Puerto serie** a 115200 baudios.
2. **ADC:** resolución de 12 bits (0–4095) y atenuación de 11 dB, que amplía el rango de entrada a
   aproximadamente 0–3,3 V (sin atenuación el ADC del ESP32 sólo mide hasta ~1,1 V).
3. **Calibración de dedos:** `cargarCalibracionNVS()` lee de la flash los valores de mano abierta y
   cerrada (ver 4.4).
4. **Bus I2C:** primero `rutinaRecuperacionI2C()` (ver 4.3) y luego `Wire.begin(21, 22, 400000)`.
5. **MPU6050:** hasta 5 intentos de `mpu.begin()`. Si responde, se configura:
   - acelerómetro ±2 g (máxima resolución; la mano no supera esa aceleración),
   - giróscopo ±500 °/s (cubre giros rápidos de muñeca sin saturar),
   - filtro paso bajo interno (DLPF) en **44 Hz** (ver 4.7),
   - y se ejecuta `calibrarIMU()`, **con la mano quieta**.
6. **ESP-NOW:** modo estación, `WiFi.setSleep(false)` (sin ahorro de energía de radio, que agrega
   latencia), registro del callback de envío y alta del receptor como *peer* (canal 0 = canal actual,
   sin cifrado).
7. **Tarea de transmisión:** `xTaskCreatePinnedToCore(tareaTransmision, ..., prioridad 2, núcleo 1)`.
   El núcleo 0 queda para la pila Wi-Fi, que en Arduino-ESP32 corre ahí por defecto.

### 4.3. Recuperación del bus I2C (`rutinaRecuperacionI2C`)

- **Problema:** si el ESP32 se reinicia en medio de una lectura, el MPU6050 puede quedar a mitad de un
  byte manteniendo SDA en bajo. El bus queda "colgado" y `mpu.begin()` falla aunque el sensor esté bien.
- **Solución:** generar manualmente hasta 9 pulsos de reloj en SCL (suficientes para que el esclavo
  termine de sacar el byte pendiente y suelte SDA) y luego una condición de STOP.
- **De dónde sale:** es el procedimiento de *bus clear* descrito en la especificación oficial de I2C de
  NXP (UM10204, sección "Bus clear") [2].

### 4.4. Calibración de los dedos en NVS

Cada operador y cada montaje del guante dan lecturas distintas, así que cada dedo tiene dos puntos de
calibración: `calMinADC[i]` (mano abierta) y `calMaxADC[i]` (puño cerrado).

- **Almacenamiento:** `Preferences` guarda los valores en la partición NVS de la flash, en el espacio
  de nombres `calib_brazo` con claves `min_0..min_4` y `max_0..max_4`. Sobreviven a reinicios y a
  reprogramaciones del firmware.
- **Valores por defecto:** si nunca se calibró, se usan 1200 (abierta) y 3200 (cerrada).
- **Procedimiento (`ejecutarCalibracionInteractiva`)**, se dispara enviando `c` por el monitor serie:
  1. Abrir la mano completamente y enviar cualquier carácter → se registra el mínimo de cada dedo
     (promedio de 32 muestras).
  2. Cerrar el puño y enviar cualquier carácter → se registra el máximo.
  3. Se guardan los 10 valores en NVS.
- **Pausa durante la calibración (desde `v2.3.0`):** mientras dura la rutina, la bandera `calibrando`
  hace que la tarea de transmisión **no lea los dedos, no transmita y no imprima telemetría**. Así la
  consola muestra sólo las instrucciones de calibración y la mano robot no copia los movimientos de
  calibrar. Como el receptor deja de recibir paquetes, a los 250 ms entra en failsafe (mano a Home) y a
  los 2 s desacopla los servos; al terminar la calibración se reanuda el envío y la mano vuelve a
  seguir al guante sola. El IMU se sigue leyendo durante la pausa para que el filtro de la muñeca no
  pierda su estado.
- **Potenciómetros invertidos:** no hace falta que el valor de "cerrado" sea mayor que el de "abierto".
  Si un potenciómetro está montado al revés, `calMax < calMin` y el mapeo igual da 25° abierto y 90°
  cerrado (ver 4.6).

### 4.5. Lectura de los dedos con sobremuestreo (`leerADCSobremuestreado`)

```cpp
for (i = 0; i < 16; i++) { acumulador += analogRead(pin); delayMicroseconds(50); }
return acumulador / 16;
```

- **Problema:** el ADC del ESP32 es ruidoso (varias cuentas de ruido) y no es perfectamente lineal.
- **Solución:** promediar 16 lecturas. Si el ruido es aleatorio e independiente entre muestras, el
  promedio de N muestras reduce su desviación estándar en un factor $\sqrt{N}$; con N = 16, el ruido baja
  a la cuarta parte. Los 50 µs entre muestras evitan tomar lecturas demasiado correlacionadas.
- **Costo:** 5 dedos × 16 muestras × (~10 µs de conversión + 50 µs de espera) ≈ 5 ms por ciclo, que
  entra holgado en los 20 ms del período.
- La alinealidad del ADC no se corrige con el promedio; la absorbe en parte la calibración de dos puntos.

### 4.6. Conversión a ángulo de dedo

```cpp
anguloDedo = map(adcRaw, calMinADC[i], calMaxADC[i], 25, 90);
paqueteSalida.anguloDedos[i] = constrain(anguloDedo, 25, 90);
```

- `map()` de Arduino es una interpolación lineal con aritmética entera:
  $\theta = 25 + (ADC - ADC_{min}) \cdot \dfrac{90 - 25}{ADC_{max} - ADC_{min}}$.
- **25° = dedo extendido, 90° = dedo flexionado.** Es una convención *lógica*; cómo se traduce a cada
  servo físico lo decide el receptor (`INVERTIR_DEDO`, ver 5.6).
- `constrain()` satura el resultado: si el operador abre o cierra más que durante la calibración, el
  ángulo no se sale del rango mecánico de la mano.

### 4.7. Muñeca: IMU y filtro complementario

La muñeca necesita dos ángulos absolutos respecto de la gravedad: **pitch** (flexión arriba/abajo) y
**roll** (rotación o pronosupinación).

#### a) Calibración en reposo (`calibrarIMU`)

Con la mano quieta se toman 500 lecturas (≈1,5 s) y se calcula:
- el **bias** (desvío en reposo) de los ejes X e Y del giróscopo. Todo giróscopo MEMS mide un valor
  distinto de cero estando quieto; si no se resta, al integrarlo el ángulo derivaría continuamente;
- el **ángulo inicial** de pitch y roll a partir del acelerómetro, para que el filtro arranque en el
  ángulo real y no en 0° (evita un transitorio de varios segundos al encender).

#### b) Ángulo por acelerómetro (`pitchAcc`, `rollAcc`)

Con la mano quieta, el acelerómetro sólo mide la gravedad, así que la inclinación sale por trigonometría:

$$\text{roll}_{acc} = \operatorname{atan2}(a_y,\ a_z) \qquad \text{pitch}_{acc} = \operatorname{atan2}\!\left(-a_x,\ \sqrt{a_y^2 + a_z^2}\right)$$

Son las fórmulas estándar de inclinación con acelerómetro de 3 ejes (ver nota de aplicación de
Freescale/NXP AN3461 [3]). Al ser cocientes, no importa si la aceleración viene en m/s² o en g.
**Limitación:** cuando la mano acelera, el acelerómetro mide gravedad + movimiento y el ángulo se
ensucia. Por eso no alcanza sólo con el acelerómetro.

#### c) Filtro complementario

$$\theta_k = \alpha\,\big(\theta_{k-1} + \omega_k\,\Delta t\big) + (1-\alpha)\,\theta_{acc,k} \qquad \alpha = 0{,}98$$

- **Idea:** el giróscopo mide velocidad angular $\omega$ con poco ruido y responde al instante, pero al
  integrarlo acumula error (deriva). El acelerómetro da un ángulo absoluto sin deriva pero ruidoso en
  movimiento. El filtro toma el ángulo integrado del giróscopo (filtrado pasa-altos) y le suma una
  pequeña corrección del acelerómetro (filtrado pasa-bajos). Los dos filtros son *complementarios*:
  suman 1 en todas las frecuencias, de ahí el nombre.
- **Constante de tiempo:** $\tau = \dfrac{\alpha\,\Delta t}{1-\alpha} = \dfrac{0{,}98 \times 0{,}02}{0{,}02} \approx 0{,}98\ \text{s}$.
  Por debajo de ~1 s manda el giróscopo (movimientos rápidos sin retardo); por encima, manda el
  acelerómetro (corrige la deriva lentamente).
- **De dónde sale:** es la formulación clásica difundida por S. Colton, *The Balance Filter* (MIT, 2007)
  [4]; su base teórica en filtros complementarios no lineales está en Mahony et al. (2008) [5].
- **`dt` real:** se mide con `micros()` en cada ciclo; si da un valor absurdo (≤0 o >100 ms) se usa 20 ms.
- **Signos:** el giróscopo se integra con el signo que hace que un giro positivo aumente el mismo ángulo
  que calcula el acelerómetro (regla de la mano derecha: `+gyro.y` aumenta pitch y `+gyro.x` aumenta
  roll). En la `v1.0.0` el signo de pitch estaba invertido (ver sección 8).

#### d) Singularidad del roll

Si el pitch se acerca a ±90° (mano apuntando hacia arriba o abajo), $a_y$ y $a_z$ tienden a cero y
`atan2(ay, az)` queda indeterminado: el roll "salta". Por eso, si $|\text{pitch}| \ge 80°$, el roll
**no se actualiza** y conserva su último valor válido. Es el equivalente práctico del bloqueo de cardán
(*gimbal lock*) de los ángulos de Euler.

#### e) Filtro interno del MPU6050 (DLPF)

El MPU6050 tiene un filtro paso bajo digital configurable. Según el mapa de registros del fabricante [6]:

| Ajuste | Retardo acelerómetro | Retardo giróscopo |
|---|---|---|
| 21 Hz (`v2.0.0`–`v2.1.0`) | 8,5 ms | 8,3 ms |
| **44 Hz (`v2.2.0`)** | **4,9 ms** | **4,8 ms** |

En la `v2.2.0` se pasó a 44 Hz: ~3,5 ms menos de retardo con un poco más de ruido de vibración, que el
filtro complementario absorbe.

#### f) Conversión a ángulo de servo

- **Pitch** — dos tramos lineales que comparten el punto neutro (mano horizontal = servo en 57°):

  | Mano (pitch − trim) | Servo vertical |
  |---|---|
  | +45° (mano arriba) o más | 25° |
  | 0° (horizontal) | 57° |
  | −25° (mano abajo) o menos | 90° |

  Se usan dos tramos porque el rango de la mano (45° arriba, 25° abajo) y el del servo alrededor del
  neutro (32° y 33°) no son simétricos. El valor se satura en 25–90° y se redondea con `lroundf`.
- **Roll** — lineal: servo = roll + 90°, saturado en 0–180°. Mano plana (roll 0°) → servo centrado en 90°.
- Los `TRIM_*` se restan antes del mapeo para compensar que el sensor no esté perfectamente horizontal en
  el guante.

### 4.8. Empaquetado y envío

1. Se completa `paqueteSalida` con firma, número de secuencia (`contadorSecuencia++`, da la vuelta en
   255) y checksum.
2. `esp_now_send()` envía los 19 bytes al receptor en modo **unicast**: la radio del receptor devuelve un
   acuse (ACK) a nivel MAC y, si no llega, la radio reintenta automáticamente.
3. `OnDataSent()` recibe el resultado de ese ACK y lo guarda en `ultimoEnvioExitoso`, que se muestra en la
   telemetría como `OK` / `NO_ACK`. La directiva `#if ESP_IDF_VERSION >= 5.0.0` elige la firma del
   callback, que cambió entre versiones del núcleo.

### 4.9. Tarea periódica (`tareaTransmision`) y telemetría

- `vTaskDelayUntil()` despierta la tarea cada 20 ms **medidos desde el despertar anterior**, no desde el
  final del trabajo. Así el período no se alarga aunque el ciclo tarde más o menos (a diferencia de un
  `delay(20)` al final del bucle). Es el mecanismo estándar de FreeRTOS para tareas periódicas [7].
- Orden de cada ciclo: IMU → *(si `calibrando`, termina acá)* → dedos → empaquetado → envío → telemetría.
  El IMU va primero para que siga actualizándose aun durante la calibración.
- **Telemetría:** cada 10 ciclos (5 Hz) imprime
  `[TX #sec] Dedos:[p, i, m, a, me] | Mñc:[V:x, R:y] | RF:OK`. Se limita a 5 Hz porque imprimir por el
  puerto serie es lento y a 50 Hz afectaría la temporización.

### 4.10. `loop()`

Corre en paralelo a la tarea de transmisión y sólo escucha el puerto serie: si llega `c`, lanza la
calibración interactiva. Duerme 100 ms entre consultas para no ocupar CPU. `loop()` tiene prioridad 1 y
la tarea de transmisión prioridad 2 en el mismo núcleo, así que cuando `loop()` activa `calibrando` la
tarea ya terminó su ciclo: el corte es limpio a partir del ciclo siguiente.

---

## 5. Receptor (`Receptor.ino`)

### 5.1. Bloque de configuración

| Constante | Valor | Significado |
|---|---|---|
| `PERIODO_CONTROL_MS` | 20 | Ciclo máximo de la tarea de control (50 Hz) |
| `TIMEOUT_FAILSAFE_MS` | 250 | Tiempo sin paquetes válidos para declarar pérdida de enlace |
| `DEADBAND_DEDOS` / `DEADBAND_MUNIECA` | 1,0° / 1,2° | Banda muerta (ver 5.6) |
| `VEL_MAX_DEDOS` | 8°/ciclo | 400 °/s |
| `VEL_MAX_MUNIECA_VERT` | 4°/ciclo | 200 °/s |
| `VEL_MAX_MUNIECA_ROT` | 5°/ciclo | 250 °/s |
| `INVERTIR_DEDO[5]` | `{true × 5}` | Sentido de montaje de cada servo de dedo (ver 5.6) |
| `HOME_DEDOS` / `HOME_MUNIECA_VERT` / `HOME_MUNIECA_ROT` | 25 / 57 / 90° | Pose segura: mano abierta, muñeca centrada |
| `LIM_*` | dedos 25–90°, muñeca 25–90° y 0–180° | Límites mecánicos absolutos |

### 5.2. Secuencia de arranque (`setup`)

1. Inicializa todas las posiciones (actual y objetivo) en la pose *Home*.
2. Reserva los 4 temporizadores LEDC para `ESP32Servo` (`ESP32PWM::allocateTimer`).
3. `acoplarServos()`: los servos arrancan energizados **en Home**, así la mano no queda floja.
4. Crea la cola FreeRTOS de **longitud 1** (ver 5.3).
5. Inicia Wi-Fi en modo estación, `WiFi.setSleep(false)` (desde `v2.1.0`), imprime su MAC (la que hay que
   copiar en `macReceptor[]` del transmisor) e inicia ESP-NOW con el callback de recepción.
6. Crea `tareaControlActuadores` en el núcleo 1 con prioridad 3.

### 5.3. Recepción (`OnDataRecv`) y cola de un elemento

```cpp
if (len != sizeof(MensajeBrazo)) return;            // tamaño incorrecto
if (!validarPaqueteBrazo(*paquete)) return;         // firma o checksum inválidos
xQueueOverwriteFromISR(colaMensajesBrazo, paquete, ...);
```

- **Dónde corre:** según la documentación de Espressif, el callback de recepción de ESP-NOW se ejecuta
  dentro de la tarea de Wi-Fi [8]. Por eso debe ser muy corto: sólo valida y copia.
- **Por qué una cola y no variables globales:** si el callback escribiera directamente las variables que
  usa la tarea de control, ésta podría leer un paquete a medio escribir (*race condition*). La cola copia
  el paquete completo de forma atómica.
- **Por qué longitud 1 con *overwrite*:** funciona como un "buzón": sólo interesa el **último** paquete.
  Si llegaran dos antes de que la tarea lea, el viejo se descarta en lugar de acumularse (acumular
  generaría retardo creciente). `xQueueOverwrite` está pensada en FreeRTOS justamente para colas de
  longitud 1 [7].

### 5.4. Acople y desacople de servos

- `acoplarServos()`: configura cada servo a 50 Hz con pulsos de **500 µs (0°) a 2400 µs (180°)**
  (≈10,6 µs por grado) y escribe la posición actual, para que al energizarse no den un salto.
- `desacoplarServos()`: `detach()` corta la señal PWM. Sin señal, el servo deja de mantener la posición
  y deja de consumir corriente y calentarse.
- La bandera `servosAcoplados` evita acoplar o desacoplar dos veces.

### 5.5. Tarea de control (`tareaControlActuadores`)

Cada iteración tiene cinco pasos.

**Paso 1 – Esperar un paquete (cambio de la `v2.2.0`).**

```cpp
if (xQueueReceive(colaMensajesBrazo, &paqueteEntrante, pdMS_TO_TICKS(20)) == pdTRUE) { ... }
```

La tarea queda bloqueada **hasta que llega un paquete o pasan 20 ms**, lo que ocurra primero. Si llega
un paquete, se procesa en ese mismo instante; si no llega, el *timeout* mantiene el ciclo a 50 Hz para
que el failsafe y el movimiento hacia Home sigan funcionando. Antes la tarea despertaba cada 20 ms fijos
y recién ahí miraba la cola, lo que sumaba en promedio 10 ms (hasta 20 ms) de espera inútil.

Al recibir un paquete: se actualiza `marcaTiempoUltimoPaquete`, se pasa a operación normal si hacía
falta (reacoplando servos) y las posiciones **objetivo** se cargan saturadas con `constrain` a los
límites mecánicos, por si llegara un valor fuera de rango.

**Paso 2 – Supervisión de enlace (máquina de estados).**

| Estado | Cuándo se entra | Qué hace |
|---|---|---|
| `ESTADO_ESPERANDO_SYNC` | Al encender | Servos en Home, esperando el primer paquete |
| `ESTADO_OPERACION_NORMAL` | Llega un paquete válido | Sigue los objetivos del guante |
| `ESTADO_FAILSAFE_SIN_SENAL` | Más de 250 ms sin paquetes válidos | Fija los objetivos en Home |

Desde failsafe se vuelve a operación normal apenas llega un paquete válido. 250 ms equivalen a ~12
paquetes perdidos seguidos: tolera interferencias breves pero reacciona rápido si el guante se apaga o
sale de alcance.

**Paso 3 – Generador de trayectoria: limitador de velocidad + banda muerta.** Ver 5.6.

**Paso 4 – Desacople en failsafe sostenido.** Si está en failsafe, todos los servos llegaron a Home y
pasaron más de 2 s **desde que se entró en failsafe**, se llama a `desacoplarServos()`.

**Paso 5 – Telemetría a 5 Hz:**
`[RX OK] D1:x D2:x D3:x D4:x D5:x | Mñc:[V:x, R:x]` con el estado `OK`, `FAILSAFE` o `SYNC_WAIT` y las
posiciones lógicas actuales (antes de la inversión de 5.6).

### 5.6. Limitador de velocidad, banda muerta e inversión de dedos

Para cada servo, en cada iteración:

```cpp
error = objetivo - actual;
if (|error| > DEADBAND) {
    actual += constrain(error, -VEL_MAX, +VEL_MAX);
    servo.write(actual);
}
```

- **Limitador de velocidad (*slew-rate limiter*):** el servo nunca avanza más de `VEL_MAX` grados por
  iteración. Ante un salto grande del objetivo (por ejemplo al reconectar o al salir de failsafe) el
  movimiento es una rampa y no un escalón. Esto limita los picos de corriente (que pueden provocar un
  reinicio por caída de tensión, *brownout*) y el esfuerzo sobre engranajes y piezas impresas. Con los
  valores de la `v2.1.0` (400 °/s dedos, 200–250 °/s muñeca) el límite queda cerca de la velocidad propia
  de los servos, de modo que en uso normal casi no agrega retardo.
- **Banda muerta (*deadband*):** si el error es menor a 1° (dedos) o 1,2° (muñeca) no se escribe nada.
  Sin esto, el ruido residual del guante haría que el servo corrija décimas de grado todo el tiempo y
  zumbe (*hunting*), calentándose. El costo es que la posición final puede quedar hasta 1° corrida.
- **Inversión de dedos (`INVERTIR_DEDO`, desde `v2.1.0`):** todo el sistema razona con la convención
  lógica 25° = abierto, 90° = cerrado. Según cómo esté montado cada servo en la mano, puede que físicamente
  cierre al **bajar** el ángulo. `anguloServoDedo()` traduce justo antes de escribir:

  ```cpp
  servo = INVERTIR_DEDO[i] ? (25 + 90 - posicion) : posicion;   // espejo dentro de 25..90°
  ```

  La inversión se aplica sólo en la escritura, así que Home, límites, failsafe y telemetría siguen
  hablando el mismo idioma lógico. Si un dedo se moviera al revés, se cambia su valor en este arreglo.

---

## 6. Cadena temporal de un movimiento

Estimación del retardo desde que el operador mueve la mano hasta que el servo recibe la nueva orden en
la `v2.2.0`. **Son cálculos a partir del código y de hojas de datos, no mediciones** sobre este hardware.

| Etapa | Retardo | Origen |
|---|---|---|
| Espera hasta el siguiente ciclo del transmisor | 0–20 ms (media 10) | Muestreo a 50 Hz |
| Filtro interno del MPU6050 (sólo muñeca) | ~5 ms | DLPF 44 Hz [6] |
| Lectura de dedos + cálculo + empaquetado | ~5 ms | Sobremuestreo 16× |
| Transmisión ESP-NOW | ~1–6 ms | Valores típicos reportados [8][9] |
| Tarea de control del receptor | < 1 ms | Despierta al llegar el paquete (`v2.2.0`) |
| Próximo pulso PWM del servo | 0–20 ms (media 10) | PWM de 50 Hz |
| Movimiento mecánico | según amplitud | ~0,1 s/60° (SG90 sin carga) |

Orden de magnitud: **~30 ms** de media desde el sensor hasta la orden al servo, más el tiempo que el
propio servo tarda en recorrer el ángulo. Para medirlo con precisión ver la sección 9.

---

## 7. Guía de ajuste de parámetros

| Síntoma | Qué tocar | Dónde |
|---|---|---|
| Un dedo de la mano se mueve al revés | `INVERTIR_DEDO[i]` de ese dedo | Receptor |
| Un dedo no llega a abrir o cerrar del todo | Recalibrar con `c` en el monitor serie | Transmisor |
| Movimientos bruscos / la fuente se cae / la placa se reinicia | Bajar `VEL_MAX_*` | Receptor |
| Movimientos lentos | Subir `VEL_MAX_*` (sin pasar la velocidad física del servo) | Receptor |
| Servos que zumban quietos | Subir `DEADBAND_*` | Receptor |
| Poca precisión fina en reposo | Bajar `DEADBAND_*` (con cuidado por el zumbido) | Receptor |
| Muñeca no centrada con la mano en reposo | `TRIM_PITCH_DEG` / `TRIM_ROLL_DEG` = ángulo leído en pose neutra | Transmisor |
| Muñeca nerviosa con vibración | Subir `ALPHA_COMPLEMENTARIO` (0,98 → 0,99) o volver DLPF a 21 Hz | Transmisor |
| Muñeca que tarda en "asentarse" en el ángulo real | Bajar `ALPHA_COMPLEMENTARIO` (0,98 → 0,96) | Transmisor |
| Recorrido de muñeca corto o excesivo | `PITCH_MANO_*`, `ROLL_MANO_MAX`, `SERVO_V_*` | Transmisor |
| La mano se va a Home sola con el guante encendido | Revisar alcance/interferencia; si hace falta subir `TIMEOUT_FAILSAFE_MS` | Receptor |
| `RF:NO_ACK` permanente en el transmisor | Verificar `macReceptor[]` contra la MAC que imprime el receptor | Transmisor |

Cada ajuste conviene registrarlo en [`docs/pruebas/REGISTRO_PRUEBAS.md`](docs/pruebas/REGISTRO_PRUEBAS.md).

---

## 8. Evolución del diseño

Esta sección documenta cómo se planteó el código en cada versión y por qué se cambió. Las versiones
están marcadas como *tags* en el repositorio (`git checkout vX.Y.Z`).

### 8.1. Arquitectura preliminar (antes de `v1.0.0`)

| Se había planteado así | Problema | Se cambió por |
|---|---|---|
| Filtro de media móvil exponencial (EMA) en el guante **y otro** en la mano | Dos filtros en cascada suman retardo de fase; además el EMA se acerca al objetivo en pasos cada vez más chicos que hacían zumbar a los servos | Limitador de velocidad + banda muerta en el receptor |
| Bucle con `delay(20)` | El período real era 20 ms + tiempo de cómputo, variable | Tareas FreeRTOS con `vTaskDelayUntil` |
| El callback de radio escribía variables globales | Lecturas de paquetes a medio escribir | Cola FreeRTOS de un elemento |
| Sin banda muerta en dedos | Zumbido y calentamiento en reposo | `DEADBAND_DEDOS` |

### 8.2. `v1.0.0` – Filtro de Kalman en la muñeca

La muñeca se estimaba con un **filtro de Kalman 1D por eje** (`KalmanFilter.h`, modelo clásico de
K. Lauszus [10]), que estima a la vez el ángulo y el sesgo del giróscopo ajustando su ganancia según
covarianzas de ruido (`Q_angle`, `Q_bias`, `R_measure`).

En las pruebas la muñeca se comportaba "como un mouse": derivaba y no volvía al mismo lugar al volver a
la misma postura. Las causas encontradas fueron:

1. **Signo del giróscopo de pitch invertido** respecto del ángulo del acelerómetro: el giróscopo empujaba
   la estimación hacia un lado y el acelerómetro hacia el otro, y el resultado dependía del historial
   del movimiento.
2. **Variable sin inicializar en el roll:** en la zona de singularidad se usaba `rawRoll` antes de
   asignarle un valor, lo que producía saltos.
3. **Arranque desde 0°** sin ángulo inicial y sin calibrar el bias, con un transitorio al encender.
4. **Mapeo con `map((long)ángulo, ...)`**, que trunca a grados enteros y no satura antes de mapear.

### 8.3. `v2.0.0` – Filtro complementario

Se reemplazó el Kalman por el **filtro complementario** de la sección 4.7. Un Kalman bien sintonizado no
es "peor", pero para dos ángulos de inclinación a 50 Hz el complementario da un resultado equivalente
en la práctica, tiene **un solo parámetro** (α) fácil de interpretar en lugar de tres covarianzas, y es
más fácil de verificar. Además se agregaron: signos de giróscopo coherentes, calibración de bias e
inicialización desde el acelerómetro, retención del roll en la singularidad, mapeo en punto flotante
saturado y ajustable (`TRIM_*`, rangos) y, en el receptor, límites mecánicos con `constrain`.

### 8.4. `v2.1.0` – Respuesta más rápida y dedos invertibles

La prueba en hardware mostró dos problemas:

| Observado | Causa | Cambio |
|---|---|---|
| La mano tardaba mucho en seguir al guante | El limitador de velocidad era más lento que los propios servos: 150 °/s dedos, 75 °/s y 100 °/s muñeca (un giro completo de muñeca tardaba ~1,8 s) | 400 °/s dedos, 200 °/s y 250 °/s muñeca |
| Al cerrar un dedo del guante, el de la mano se abría | El sentido de montaje de los servos era opuesto a la convención 25° = abierto | `INVERTIR_DEDO[]` por dedo |
| — | El receptor no desactivaba el ahorro de energía de radio | `WiFi.setSleep(false)` también en el receptor |

### 8.5. `v2.2.0` – Menor latencia (versión definitiva)

Surgió de una revisión de proyectos y bibliografía sobre teleoperación con ESP-NOW. Se aplicaron las dos
mejoras de menor riesgo:

| Antes | Después | Ganancia estimada |
|---|---|---|
| Tarea del receptor despertaba cada 20 ms fijos y recién ahí leía la cola | Se bloquea en la cola y despierta al llegar el paquete (timeout 20 ms) | ~10 ms de media |
| DLPF del MPU6050 en 21 Hz | 44 Hz | ~3,5 ms |

Resultado de la prueba: funcionamiento correcto, adoptada como versión definitiva hasta nuevas pruebas.

### 8.6. `v2.3.0` – Calibración sin transmisión

Pedido del equipo tras usar la `v2.2.0`: al calibrar, la telemetría a 5 Hz se mezclaba con las
instrucciones en la consola y la mano robot se movía siguiendo los gestos de calibración. Se agregó la
bandera `calibrando` (ver 4.4): durante la calibración el transmisor no lee dedos, no envía ni imprime;
la mano pasa a failsafe y se recupera sola al terminar.

---

## 9. Observaciones conocidas y mejoras futuras

Puntos detectados en la revisión del código que **no afectan el funcionamiento actual**, pero conviene
conocer:

1. **`xQueueOverwriteFromISR` desde el callback de ESP-NOW.** El callback corre en la tarea de Wi-Fi, no en
   una interrupción [8]; la variante canónica sería `xQueueOverwrite`. Funciona en el ESP32, pero si se
   toca esa parte conviene cambiarlo.
2. **Limitador por iteración, no por tiempo.** Desde `v2.2.0` la tarea itera al ritmo de los paquetes
   (50 Hz). Si en el futuro se aumenta la tasa del transmisor, `VEL_MAX_*` debería escalarse con el `dt`
   real; si no, la velocidad máxima subiría en la misma proporción.
3. **Rama redundante en el mapeo de dedos.** El `else` para potenciómetros invertidos hace lo mismo que la
   rama principal, porque `map()` ya admite rangos invertidos. Es inofensivo.
4. **Calibración concurrente (resuelto en `v2.3.0`).** Antes la calibración corría mientras la tarea
   seguía leyendo el ADC, transmitiendo e imprimiendo, lo que mezclaba la telemetría con las
   instrucciones y hacía que la mano copiara los movimientos de calibrar. Ahora se pausa (ver 4.4).
5. **MAC del receptor fija en el código.** Si se cambia la placa receptora hay que actualizar
   `macReceptor[]` y reprogramar el transmisor.

Mejoras propuestas por la investigación de latencia, **no implementadas** (requieren medición previa):

- Filtro *One Euro* [11] en lugar del limitador de velocidad: filtra fuerte en reposo y casi nada en
  movimientos rápidos.
- Subir la tasa de envío a 100 Hz (escalando el limitador, ver punto 2).
- PWM de 100–200 Hz en los MG946R (el fabricante sólo especifica 50 Hz; probar vigilando temperatura).
- **Medir la latencia real** de punta a punta: incluir `micros()` del transmisor en el paquete o
  conmutar un GPIO en cada extremo y observar ambos con un osciloscopio.

---

## 10. Referencias

1. J. G. Fletcher, "An Arithmetic Checksum for Serial Transmissions", *IEEE Transactions on
   Communications*, vol. 30, n.º 1, pp. 247–252, 1982.
2. NXP Semiconductors, *UM10204 – I2C-bus specification and user manual*, sección "Bus clear".
   https://www.nxp.com/docs/en/user-guide/UM10204.pdf
3. M. Pedley, *AN3461 – Tilt Sensing Using a Three-Axis Accelerometer*, Freescale Semiconductor, 2013.
4. S. Colton, *The Balance Filter: A Simple Solution for Integrating Accelerometer and Gyroscope
   Measurements for a Balancing Platform*, MIT, 2007.
5. R. Mahony, T. Hamel, J.-M. Pflimlin, "Nonlinear Complementary Filters on the Special Orthogonal
   Group", *IEEE Transactions on Automatic Control*, vol. 53, n.º 5, 2008.
6. InvenSense, *MPU-6000 and MPU-6050 Register Map and Descriptions*, registro 26 (CONFIG / DLPF_CFG).
7. FreeRTOS, documentación de `vTaskDelayUntil`, `xQueueReceive` y `xQueueOverwrite`. https://www.freertos.org
8. Espressif Systems, *ESP-IDF Programming Guide – ESP-NOW* y *ESP-FAQ – ESP-NOW*.
   https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_now.html
   · https://docs.espressif.com/projects/esp-faq/en/latest/application-solution/esp-now.html
9. Espressif, issue de GitHub sobre latencia de ESP-NOW con envíos frecuentes:
   https://github.com/espressif/esp-now/issues/115
10. K. S. Lauszus (TKJ Electronics), *KalmanFilter* para IMU. https://github.com/TKJElectronics/KalmanFilter
11. G. Casiez, N. Roussel, D. Vogel, "1€ Filter: A Simple Speed-based Low-pass Filter for Noisy Input in
    Interactive Systems", CHI 2012. https://cristal.univ-lille.fr/~casiez/1euro
