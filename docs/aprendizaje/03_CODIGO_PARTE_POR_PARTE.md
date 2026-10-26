# 3. El código parte por parte: qué hace y por qué

[← Volver al índice](README.md)

Cada bloque tiene tres cosas: **qué hace** en una línea, **por qué se hizo así** en frases para decir en la
exposición, y el enlace al detalle en [`EXPLICACION_CODIGO.md`](../../EXPLICACION_CODIGO.md) (abreviado
*EC*). FreeRTOS y ESP-NOW tienen su propio archivo; acá sólo se mencionan.

---

## 3.1. El protocolo (`ProtocoloBrazo.h`)

Es el "contrato" entre las dos placas: define cómo es el paquete y cómo se verifica. Hay **dos copias
idénticas**, una por carpeta, porque el IDE de Arduino sólo compila los archivos de la carpeta del sketch.

### La estructura del paquete

```cpp
#pragma pack(push, 1)
struct MensajeBrazo {
    uint16_t magicHeader;        // 0xAA55          2 bytes
    uint8_t  secuencia;          // 0..255          1 byte
    uint16_t anguloDedos[5];     // 25..90°        10 bytes
    int16_t  muniecaVertical;    // 25..90°         2 bytes
    int16_t  muniecaRotacional;  // 0..180°         2 bytes
    uint16_t checksum;           // Fletcher-16     2 bytes   → 19 bytes
};
#pragma pack(pop)
```

- **Se mandan ángulos de servo ya calculados, no lecturas crudas**, porque así toda la calibración vive en
  el guante y el receptor no necesita saber nada del operador.
- **Se usó `#pragma pack(1)`** porque el compilador, por defecto, agrega bytes de relleno para alinear los
  campos de 16 bits. Sin relleno, la estructura mide exactamente 19 bytes y tiene la misma forma en
  memoria en las dos placas: se puede mandar tal cual y reinterpretar del otro lado sin convertir campo
  por campo.
- **Se usaron tipos de tamaño fijo** (`uint16_t`, `int16_t`) porque un `int` puede medir distinto según el
  compilador; acá cada campo tiene un tamaño garantizado.
- **Enteros y no `float`:** un grado de resolución alcanza para un servo y ocupa la mitad.

### La firma `0xAA55`

Un valor fijo al principio. Si no coincide, el paquete no es nuestro. **Se eligió `0xAA55`** porque en
binario alterna unos y ceros (`10101010 01010101`), un patrón muy improbable por azar. Es una convención
habitual en protocolos y sectores de arranque.

### El checksum Fletcher-16

```cpp
sum1 = (sum1 + byte) % 255;   // suma de los bytes
sum2 = (sum2 + sum1) % 255;   // suma de las sumas: depende de la POSICIÓN de cada byte
```

- **Se usaron dos sumas** porque una suma simple no detecta dos bytes intercambiados; la segunda suma sí.
- **Se eligió Fletcher y no CRC** porque son sólo sumas, sin tablas ni polinomios, y para 17 bytes alcanza.
  La radio ya trae su propio CRC-32; esto es una segunda barrera en nuestra aplicación.

Detalle: *EC* §3.

---

## 3.2. Transmisor (`Transmisor.ino`): el guante

### Orden de `setup()` y por qué ese orden

1. **Serie** primero, para poder ver los mensajes de todo lo demás.
2. **ADC y calibración de dedos (NVS)**, que no dependen de nada.
3. **I2C + MPU6050**, con recuperación del bus y reintentos.
4. **Calibración de la IMU**, que necesita el MPU ya configurado.
5. **Radio y ESP-NOW.**
6. **Tarea periódica al final**, para que arranque con todo ya inicializado.

### Configuración del ADC

```cpp
analogReadResolution(12);         // 0..4095
analogSetAttenuation(ADC_11db);   // rango de entrada ~0..3,3 V
```

- **Se usó 11 dB de atenuación** porque sin ella el ADC del ESP32 sólo mide hasta ~1,1 V, y los
  potenciómetros recorren de 0 a 3,3 V.
- **Pines del ADC1**, porque el ADC2 no funciona con la radio encendida (ver
  [ESP-NOW §2.3](02_ESPNOW.md#23-cómo-lo-usamos-transmisor)).

### Lectura de dedos con sobremuestreo (`leerADCSobremuestreado`)

Promedia 16 lecturas separadas por 50 µs.

- **Se promedian 16 muestras** porque el ADC del ESP32 es ruidoso. Promediar N muestras reduce el ruido
  aleatorio en √N: con 16, a la cuarta parte.
- **Costo:** ~5 ms por ciclo para los cinco dedos, que entran de sobra en los 20 ms.
- En la calibración se usan 32 muestras, porque ahí no hay apuro y conviene el valor más estable.

### Calibración de dedos en NVS (`Preferences`)

Guarda para cada dedo la lectura con mano abierta y con puño cerrado.

- **Se calibra por dedo y por operador** porque cada mano y cada montaje del potenciómetro dan lecturas
  distintas.
- **Se usó `Preferences`** (memoria no volátil, NVS) porque los valores sobreviven a reinicios y a
  reprogramar el firmware: se calibra una vez, no cada vez que se enciende.
- **Se pausa la transmisión mientras se calibra** (`calibrando`, desde `v2.3.0`) para que la mano no copie
  los movimientos de calibrar y la consola muestre sólo las instrucciones. La mano entra en failsafe sola
  y se recupera al terminar.

### Conversión a ángulo de dedo

```cpp
anguloDedo = map(adcRaw, calMinADC[i], calMaxADC[i], 25, 90);
anguloDedos[i] = constrain(anguloDedo, 25, 90);
```

- **Se usó `map()`** porque la relación potenciómetro-ángulo es aproximadamente lineal y con dos puntos
  de calibración alcanza.
- **Se usó `constrain()`** para que, si el operador abre o cierra más que al calibrar, el ángulo nunca
  salga del rango mecánico de la mano.
- **25° = abierto, 90° = cerrado** es una convención lógica; el receptor la traduce a cada servo real.

### Recuperación del bus I2C (`rutinaRecuperacionI2C`)

Antes de iniciar el I2C, genera 9 pulsos de reloj y una condición de STOP.

- **Se hizo** porque si el ESP32 se reinicia a mitad de una lectura, el MPU6050 puede quedar sosteniendo
  la línea SDA en bajo y el bus queda colgado. Los 9 pulsos le permiten terminar el byte pendiente y
  soltarla. Es el procedimiento *bus clear* de la especificación oficial de I2C.
- **Reintentos de `mpu.begin()` (5)** por el mismo motivo: un fallo transitorio al arrancar no debería
  dejar el guante sin muñeca.

### Configuración de la IMU

| Ajuste | Valor | Por qué |
|---|---|---|
| Acelerómetro | ±2 g | Máxima resolución; la mano no supera esa aceleración |
| Giróscopo | ±500 °/s | Cubre giros rápidos de muñeca sin saturar |
| Filtro interno (DLPF) | 44 Hz | ~4,9 ms de retardo, contra ~8,5 ms a 21 Hz (`v2.2.0`) |
| I2C | 400 kHz | Modo rápido: la lectura del sensor tarda menos |

### Calibración de la IMU en reposo (`calibrarIMU`)

Con la mano quieta, promedia 500 lecturas (~1,5 s).

- **Se mide el *bias* del giróscopo** porque todo giróscopo MEMS marca algo distinto de cero estando
  quieto. Si no se resta, al integrarlo el ángulo deriva sin parar.
- **Se arranca el filtro en el ángulo del acelerómetro** y no en 0° para que no haya un transitorio de
  varios segundos al encender.

### Filtro complementario (la muñeca)

```cpp
angulo = 0.98 * (angulo + gyro * dt) + 0.02 * anguloAcelerometro;
```

- **Se usó porque cada sensor falla donde el otro es bueno:** el giróscopo responde rápido y con poco
  ruido, pero al integrarlo deriva; el acelerómetro da un ángulo absoluto sin deriva, pero se ensucia
  cuando la mano acelera. El filtro toma lo rápido del giróscopo y lo corrige lentamente con el
  acelerómetro.
- **α = 0,98 da una constante de tiempo de ~1 s:** por debajo manda el giróscopo, por encima el
  acelerómetro.
- **Se reemplazó al filtro de Kalman de la `v1.0.0`**, que tenía errores de signo y de inicialización y
  hacía que la muñeca se comportara "como un mouse". El complementario es más simple, tiene un solo
  parámetro y se entiende de un vistazo. Ver *EC* §8.
- **`dt` se mide con `micros()`** en vez de suponer 20 ms, y si da un valor absurdo se usa 20 ms.
- **El roll se congela con |pitch| ≥ 80°** porque ahí la fórmula del acelerómetro queda indeterminada (la
  mano apunta hacia arriba o abajo).

Detalle y fórmulas: *EC* §4.7.

### Conversión de la muñeca a ángulo de servo

- **Pitch en dos tramos lineales** alrededor del neutro (mano horizontal = 57°), porque el rango de la mano
  (45° arriba, 25° abajo) no es simétrico.
- **Roll lineal:** servo = roll + 90°. Mano plana = servo centrado.
- **`TRIM_*`** para compensar que el sensor no quede perfectamente horizontal en el guante.

### Empaquetado, envío y telemetría

- **Secuencia** que se incrementa en cada paquete; **checksum** al final, calculado sobre todo lo demás.
- **Telemetría cada 10 ciclos (5 Hz)** porque imprimir por serie es lento: a 50 Hz afectaría la
  temporización y además no se podría leer.

### Orden dentro de cada ciclo

IMU → *(si se está calibrando, termina acá)* → dedos → paquete → envío → telemetría.
**La IMU va primero** para que el filtro siga actualizado incluso durante la calibración.

---

## 3.3. Receptor (`Receptor.ino`): la mano

### Generación del PWM de los servos (`ESP32Servo`)

```cpp
ESP32PWM::allocateTimer(0..3);
servo.setPeriodHertz(50);
servo.attach(pin, 500, 2400);   // 500 µs = 0°, 2400 µs = 180°
```

- **Se usó `ESP32Servo`** porque la librería `Servo` clásica de Arduino no funciona en el ESP32; ésta usa el
  periférico de PWM por hardware del chip (LEDC), así que los pulsos no dependen de la CPU.
- **Se reservan los 4 temporizadores del LEDC** para que la librería los tenga disponibles al generar los
  siete canales de PWM.
- **50 Hz y 500–2400 µs** son la frecuencia y el rango de pulsos que esperan los servos SG90 y MG946R.

### Acople y desacople (`acoplarServos` / `desacoplarServos`)

- **Al acoplar se escribe primero la posición actual**, para que el servo no pegue un salto al energizarse.
- **Se desacopla tras 2 s en Home en failsafe** porque sin señal PWM el servo deja de hacer fuerza: no
  consume ni se calienta mientras nadie lo usa.
- **La bandera `servosAcoplados`** evita acoplar o desacoplar dos veces.

### Máquina de estados (failsafe)

| Estado | Entra cuando | Hace |
|---|---|---|
| `ESPERANDO_SYNC` | Al encender | Servos en Home, esperando el primer paquete |
| `OPERACION_NORMAL` | Llega un paquete válido | Sigue al guante |
| `FAILSAFE_SIN_SENAL` | 250 ms sin paquetes válidos | Lleva todo a Home; a los 2 s desacopla |

- **Se usó una máquina de estados** porque el comportamiento cambia según haya o no enlace, y así queda
  explícito y fácil de seguir.
- **250 ms** son ~12 paquetes perdidos seguidos: tolera interferencias breves pero reacciona rápido si el
  guante se apaga o sale de alcance.
- **Home = mano abierta y muñeca neutra**, la posición de reposo más segura: en el centro del recorrido de
  la muñeca y sin cerrar los dedos sobre nada.

### Validación de los objetivos

```cpp
posObjetivoDedos[i] = constrain(paquete.anguloDedos[i], 25, 90);
```

**Se satura otra vez en el receptor**, aunque el transmisor ya lo haga, porque la mano no debe confiar en
lo que llega: si alguna vez llegara un valor fuera de rango, la pieza impresa no se fuerza.

### Generador de trayectoria: límite de velocidad + banda muerta

```cpp
error = objetivo - actual;
if (|error| > DEADBAND) {
    actual += constrain(error, -VEL_MAX, +VEL_MAX);
    servo.write(actual);
}
```

- **Límite de velocidad (*slew-rate*):** cada servo avanza como máximo `VEL_MAX` grados por ciclo (8° en
  dedos = 400 °/s; 4–5° en muñeca). **Se usó** para que un salto grande del objetivo (al reconectar o al
  salir de failsafe) sea una rampa y no un golpe: protege engranajes y piezas impresas y evita picos de
  corriente que podrían reiniciar la placa por caída de tensión. Los valores están cerca de la velocidad
  propia del servo, así que en uso normal casi no agregan retardo.
- **Banda muerta (*deadband*):** errores menores a 1° (dedos) o 1,2° (muñeca) se ignoran. **Se usó**
  porque el pequeño ruido residual del guante haría que el servo corrija décimas de grado todo el tiempo,
  zumbe y se caliente. El costo es que la posición final puede quedar hasta 1° corrida.
- **Posiciones en `float`**, para que los incrementos fraccionarios se acumulen bien; se redondea recién al
  escribir el servo.

### Inversión de dedos (`INVERTIR_DEDO`)

```cpp
servo = INVERTIR_DEDO[i] ? (25 + 90 - pos) : pos;
```

**Se hizo** porque según cómo quedó montado cada servo en la mano, puede cerrar el dedo al bajar el ángulo
en lugar de subirlo. **La inversión se aplica sólo al escribir el servo**, así que Home, límites, failsafe
y telemetría siguen usando la misma convención lógica (25 = abierto).

---

## 3.4. Decisiones y su costo (para responder "¿y por qué no...?")

| Decisión | Qué ganamos | Qué cuesta |
|---|---|---|
| Ángulos calculados en el guante | Receptor simple; cambiar de operador = recalibrar el guante | El guante tiene que conocer los rangos de la mano |
| Paquete binario empaquetado | 19 bytes, sin conversión | Ambas placas deben tener el mismo `ProtocoloBrazo.h` |
| Fletcher-16 | Simple, sólo sumas | Algo menos robusto que un CRC-16 |
| Sobremuestreo 16× | Ruido ÷4 | ~5 ms del ciclo |
| Filtro complementario | Simple, sin deriva, un solo parámetro | No modela el ruido como un Kalman |
| DLPF de 44 Hz | ~3,5 ms menos de retardo | Un poco más de ruido de vibración |
| 50 Hz de envío | Coincide con el PWM de los servos | Hasta 20 ms de espera hasta el próximo ciclo |
| Cola de 1 elemento con overwrite | Siempre el último dato, sin retardo acumulado | Se descartan paquetes intermedios (a propósito) |
| Límite de velocidad | Protege la mecánica y la alimentación | Movimientos muy bruscos se suavizan |
| Banda muerta | Sin zumbido ni calentamiento | Hasta 1° de error final |
| Failsafe a 250 ms | Seguro ante pérdida de enlace | Una interferencia larga manda la mano a Home |
| Sin cifrado ni filtro por MAC | Configuración simple | Otro dispositivo que conozca el formato podría comandar la mano |

---

## 3.5. Preguntas de práctica

**¿Por qué el transmisor manda ángulos y no las lecturas crudas?**
Para que toda la calibración del operador viva en el guante. El receptor sólo mueve motores con
seguridad y no necesita saber nada de quién lo usa.

**¿Para qué sirve `#pragma pack(1)`?**
Para que el compilador no agregue bytes de relleno. Así la estructura mide 19 bytes y tiene la misma forma
en las dos placas, y se puede mandar tal cual.

**¿Por qué Fletcher y no CRC?**
Es más simple (sólo sumas) y alcanza para 17 bytes. La radio ya tiene su CRC-32; el nuestro es una segunda
barrera en la aplicación.

**¿Por qué promedian 16 lecturas del ADC?**
Porque el ADC del ESP32 es ruidoso. Promediar 16 muestras reduce el ruido aleatorio a la cuarta parte y
cuesta unos 5 ms de los 20 disponibles.

**¿Por qué no usaron sólo el acelerómetro o sólo el giróscopo?**
El acelerómetro se ensucia cuando la mano se mueve; el giróscopo deriva al integrarlo. El filtro
complementario combina lo bueno de cada uno.

**¿Por qué cambiaron el Kalman por el complementario?**
La implementación de la `v1.0.0` tenía un signo invertido y una variable sin inicializar, y la muñeca
derivaba. El complementario resolvió el problema con un solo parámetro y es más fácil de verificar.

**¿Qué es el *bias* del giróscopo?**
Lo que marca estando quieto. Hay que restarlo porque, al integrarlo, haría que el ángulo crezca solo.

**¿Por qué la mano tiene que estar quieta al encender el guante?**
Porque en ese momento se mide el *bias* del giróscopo y el ángulo inicial. Si se mueve, los dos quedan mal.

**¿Para qué sirve la recuperación del bus I2C?**
Si la placa se reinicia en medio de una lectura, el sensor puede quedar trabando la línea de datos. Los
9 pulsos de reloj lo destraban.

**¿Por qué el receptor vuelve a limitar los ángulos si el transmisor ya lo hace?**
Porque la mano no debe confiar en lo que recibe. Es la última protección de la mecánica.

**¿Qué es el *slew-rate* y por qué lo usan?**
Un límite de cuántos grados avanza cada servo por ciclo. Evita golpes mecánicos y picos de corriente
cuando el objetivo cambia de golpe.

**¿Por qué la banda muerta?**
Sin ella, el ruido residual haría que el servo corrija todo el tiempo, zumbe y se caliente.

**¿Qué pasa si se corta la señal?**
A los 250 ms la mano va suavemente a Home (abierta, muñeca neutra) y a los 2 s desacopla los servos. Al
volver la señal, sigue al guante sola.

**¿Por qué desacoplan los servos en failsafe?**
Para que no hagan fuerza ni se calienten mientras nadie los usa.

**¿Por qué existe `INVERTIR_DEDO`?**
Porque algunos servos quedaron montados al revés. La inversión se aplica sólo al escribir el servo, así el
resto del código sigue con la misma convención.

**¿Cuánta latencia tiene el sistema?**
Estimamos ~30 ms de media desde el sensor hasta la orden al servo, más lo que tarda el servo en moverse.
**Es un cálculo a partir del código, no una medición**; medirla es uno de los próximos pasos.
