# 1. FreeRTOS: qué es y cómo lo usamos

[← Volver al índice](README.md)

## 1.1. Qué es y por qué está ahí

**FreeRTOS** es un sistema operativo de tiempo real (*RTOS*) muy chico, pensado para microcontroladores.
"Tiempo real" no significa "rápido": significa **predecible**. Lo que importa es que una tarea que
tiene que correr cada 20 ms corra cada 20 ms, siempre, aunque haya otras cosas pasando.

Dato clave para la exposición: **no lo agregamos nosotros**. El ESP32 ya trae FreeRTOS adentro: el
ESP-IDF (el SDK de Espressif) está construido sobre él y el core de Arduino para ESP32 corre encima del
ESP-IDF. Incluso `setup()` y `loop()` son, por debajo, una tarea de FreeRTOS llamada `loopTask`. Por eso
podemos llamar a `xTaskCreatePinnedToCore()` o `xQueueCreate()` sin instalar ninguna librería.

> **Frase para la defensa:** "Usamos FreeRTOS porque ya viene en el ESP32 y nos permite separar el
> trabajo en tareas con período fijo y prioridad, en lugar de meter todo en un `loop()` que se atrasa
> cuando algo tarda más de lo esperado."

### La alternativa que no usamos: el *superloop*

En Arduino clásico todo va en `loop()` y se temporiza con `millis()`:

```cpp
void loop() {
    if (millis() - ultimo >= 20) { ultimo = millis(); leer(); enviar(); }
    atenderSerie();   // si esto tarda 30 ms, el envío se atrasa
}
```

Funciona en casos simples, pero el período depende de todo lo demás que haya en el bucle: un
`Serial.print` largo o una calibración interactiva atrasan el envío. Con tareas, el sistema operativo se
encarga de que la tarea importante corra a tiempo aunque otra parte del programa esté ocupada.

---

## 1.2. Conceptos que hay que tener claros

### Tareas

Una **tarea** es una función con un bucle infinito que el sistema operativo ejecuta como si fuera un
programa independiente. Cada una tiene su **propia pila** (memoria para variables locales y llamadas).

```cpp
void miTarea(void* parametros) {
    for (;;) {          // nunca retorna
        // trabajo
        // ...y SIEMPRE algo que bloquee (vTaskDelay, xQueueReceive, etc.)
    }
}
```

Una tarea está siempre en uno de estos estados:

| Estado | Significado |
|---|---|
| *Running* | Está usando la CPU ahora |
| *Ready* | Quiere correr, pero hay otra de mayor prioridad corriendo |
| *Blocked* | Está esperando algo (que pase un tiempo o que llegue un dato). **No gasta CPU** |
| *Suspended* | Pausada a mano (no lo usamos) |

### Scheduler y prioridades

El **scheduler** decide qué tarea corre. FreeRTOS es **preemptivo por prioridad**: en cada núcleo corre
siempre la tarea *Ready* de mayor prioridad. Si una tarea de prioridad alta se despierta, interrumpe
(*preempta*) a la de menor prioridad en ese mismo instante, sin esperar a que ésta termine.

Consecuencia importante: **una tarea de prioridad alta que nunca se bloquea no deja correr a ninguna de
menor prioridad.** Por eso todos nuestros bucles infinitos tienen una espera (`vTaskDelayUntil`,
`xQueueReceive` con timeout o `vTaskDelay`). En cada núcleo existe además una tarea *Idle* de prioridad 0
que corre cuando no hay nada más que hacer.

### El tick

FreeRTOS mide el tiempo en **ticks**. En el core Arduino del ESP32 el tick es de **1 ms** (1000 Hz).
`pdMS_TO_TICKS(20)` convierte 20 ms a ticks; se usa para que el código siga siendo correcto si alguien
cambia la frecuencia del tick.

### Dos núcleos

El ESP32 tiene **dos núcleos** (0 y 1). Cada uno tiene su propio scheduler corriendo. Por defecto:

- **Núcleo 0:** la pila Wi-Fi (y por lo tanto ESP-NOW), con prioridad alta.
- **Núcleo 1:** `loopTask` (es decir, `setup()` y `loop()`), con prioridad 1.

`xTaskCreatePinnedToCore()` permite elegir en qué núcleo corre cada tarea. Si no se fija, la tarea puede
caer en cualquiera.

### Colas

Una **cola** es un canal para pasar datos de una tarea (o de un callback) a otra. La clave es que
**copia el dato completo** dentro de una sección protegida: quien lee nunca ve un dato a medio escribir.
Además, una tarea puede **bloquearse esperando** que llegue algo a la cola, sin gastar CPU.

### Condición de carrera (*race condition*)

Es el problema que las colas evitan. Ejemplo con nuestro paquete de 19 bytes:

1. El callback de radio empieza a copiar un paquete nuevo en una variable global.
2. A mitad de la copia, la tarea de control lee esa variable.
3. La tarea obtiene los dedos del paquete nuevo y la muñeca del viejo: un paquete que nunca existió.

En un sistema con dos núcleos esto no es teórico: las dos cosas pueden ocurrir **literalmente al mismo
tiempo**, una en cada núcleo.

### `volatile`

Le dice al compilador "esta variable puede cambiar por fuera de esta función; leela siempre de memoria".
Sin `volatile`, el compilador podría guardar el valor en un registro y no ver nunca el cambio. **Sirve
para banderas simples** (un `bool`); no reemplaza a una cola para datos de varios bytes.

---

## 1.3. Cómo lo usamos en el transmisor

### Mapa de tareas

| Tarea | Núcleo | Prioridad | Qué hace | Cómo espera |
|---|---|---|---|---|
| `tareaTransmision` | 1 | 2 | IMU, dedos, paquete, envío, telemetría | `vTaskDelayUntil` cada 20 ms |
| `loop()` (`loopTask`) | 1 | 1 | Atiende el puerto serie (comando `c`) | `vTaskDelay(100 ms)` |
| Wi-Fi / ESP-NOW | 0 | alta | Pila de radio, llama a `OnDataSent` | (del sistema) |

### Creación de la tarea

```cpp
xTaskCreatePinnedToCore(
    tareaTransmision,   // función de la tarea
    "TaskTxBrazo",      // nombre (sólo para depurar)
    4096,               // tamaño de pila, en BYTES (en ESP-IDF; en FreeRTOS "puro" son palabras)
    NULL,               // parámetro que recibe la tarea (no usamos)
    2,                  // prioridad
    NULL,               // handle para manejarla después (no lo necesitamos)
    1                   // núcleo
);
```

- **Se fijó al núcleo 1** para que la actividad de la radio, que vive en el núcleo 0, no interrumpa el
  ciclo de medición y no le agregue variación (*jitter*) al período.
- **Prioridad 2** para que esté por encima de `loop()` (prioridad 1). Esto importa para la calibración
  (ver más abajo).
- **4096 bytes de pila** es un valor habitual para una tarea con `printf` y variables locales chicas. No
  se midió el uso real; se podría verificar con `uxTaskGetStackHighWaterMark()`.

### Período exacto con `vTaskDelayUntil`

```cpp
TickType_t xLastWakeTime = xTaskGetTickCount();
for (;;) {
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(20));
    // ... trabajo (~5 ms) ...
}
```

- `vTaskDelayUntil` **despierta en instantes absolutos**: t0+20, t0+40, t0+60 ms... Guarda en
  `xLastWakeTime` cuándo despertó y calcula el siguiente a partir de ahí.
- Si usáramos `vTaskDelay(20)` al final, el período sería 20 ms **más** lo que tarda el trabajo
  (~25 ms → 40 Hz) y además variaría de un ciclo a otro.

> **Frase:** "Se usó `vTaskDelayUntil` porque mide el período desde el despertar anterior y no desde el
> final del trabajo; así los 50 Hz son exactos aunque el cálculo tarde más o menos en cada ciclo."

### `loop()` sigue existiendo, con otro rol

`loop()` quedó sólo para leer el puerto serie. Se usa `vTaskDelay(100)` al final para que se bloquee y no
gire en vacío: revisar el teclado 10 veces por segundo alcanza de sobra.

### Calibración: un ejemplo de por qué importan las prioridades

Cuando el usuario manda `c`, `loop()` ejecuta la calibración y pone `calibrando = true`. La tarea de
transmisión ve esa bandera y deja de leer dedos, transmitir e imprimir.

¿Por qué es seguro cambiar la bandera sin más protección? Porque **las dos tareas están en el mismo
núcleo y la de transmisión tiene más prioridad**. `loop()` sólo puede estar corriendo cuando la tarea de
transmisión está bloqueada esperando su próximo ciclo; nunca la agarra a mitad de un envío. En el ciclo
siguiente, la tarea lee la bandera y se detiene. La bandera es `volatile` porque la escribe una tarea y la
lee otra.

Durante la calibración, los `delay(50)` de la espera de teclado **también bloquean** (en el ESP32,
`delay()` llama a `vTaskDelay()`), así que la tarea de transmisión sigue corriendo y mantiene actualizado
el filtro de la IMU.

### `volatile bool ultimoEnvioExitoso`

La escribe el callback `OnDataSent` (tarea Wi-Fi, núcleo 0) y la lee la tarea de transmisión para la
telemetría (núcleo 1). Es un `bool`: se escribe en una sola operación, así que `volatile` alcanza y no
hace falta una cola.

---

## 1.4. Cómo lo usamos en el receptor

### Mapa de tareas

| Tarea | Núcleo | Prioridad | Qué hace | Cómo espera |
|---|---|---|---|---|
| Callback `OnDataRecv` | 0 | (tarea Wi-Fi) | Valida el paquete y lo pone en la cola | — |
| `tareaControlActuadores` | 1 | 3 | Failsafe, trayectoria, escritura de servos | `xQueueReceive` con timeout de 20 ms |
| `loop()` | 1 | 1 | Nada | `vTaskDelay(500 ms)` |

El número exacto de prioridad (3) no es crítico: lo importante es que esté por encima de `loop()` y de
*Idle*, y que no compita con la Wi-Fi porque está en el otro núcleo.

### La cola de un elemento: un "buzón"

```cpp
colaMensajesBrazo = xQueueCreate(1, sizeof(MensajeBrazo));   // 1 lugar de 19 bytes
```

En el callback de recepción:

```cpp
BaseType_t xHigherPriorityTaskWoken = pdFALSE;
xQueueOverwriteFromISR(colaMensajesBrazo, paquete, &xHigherPriorityTaskWoken);
portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
```

- **Por qué una cola y no una variable global:** para evitar la condición de carrera de la sección 1.2.
  El callback corre en el núcleo 0 y la tarea de control en el núcleo 1, al mismo tiempo.
- **Por qué longitud 1:** a la mano sólo le interesa **el último paquete**. Una cola larga acumularía
  paquetes viejos y la mano iría cada vez más atrasada respecto del guante.
- **Por qué `Overwrite`:** si llega un paquete nuevo antes de que la tarea lea el anterior, lo reemplaza.
  Nunca falla ni espera. FreeRTOS la ofrece justamente para colas de un elemento.
- **Qué hacen `xHigherPriorityTaskWoken` y `portYIELD_FROM_ISR`:** es el patrón estándar de FreeRTOS para
  rutinas de interrupción. La función de la cola avisa si despertó una tarea más prioritaria, y el
  `portYIELD` pide al scheduler que cambie de tarea en ese momento en lugar de esperar al próximo tick.

> **Matiz honesto (por si preguntan):** el callback de ESP-NOW **no es una interrupción de hardware**:
> según Espressif corre dentro de la tarea de Wi-Fi. La variante "de libro" sería `xQueueOverwrite` (sin
> `FromISR`). La que usamos funciona en el ESP32 y no bloquea nunca; está anotado como mejora en la
> sección 9 de `EXPLICACION_CODIGO.md`.

### Esperar un paquete con timeout: eventos + período en una sola línea

```cpp
if (xQueueReceive(colaMensajesBrazo, &paqueteEntrante, pdMS_TO_TICKS(20)) == pdTRUE) {
    // llegó un paquete: actualizar objetivos
}
// siempre: failsafe, trayectoria, servos
```

La tarea queda bloqueada **hasta que llega un paquete o pasan 20 ms, lo que ocurra primero**:

- **Si llega un paquete**, despierta en ese instante y lo procesa. No espera al próximo tick de 50 Hz.
  Esto es el cambio de la `v2.2.0`, que sacó en promedio 10 ms de latencia.
- **Si no llega nada**, el timeout la despierta igual cada 20 ms. Así el failsafe (detectar que pasaron
  250 ms sin paquetes) y el movimiento suave hacia Home siguen funcionando aunque el guante esté apagado.

> **Frase:** "Se usó `xQueueReceive` con timeout porque en una sola llamada la tarea reacciona apenas
> llega un paquete y, si no llega ninguno, igual se despierta a 50 Hz para vigilar el enlace."

### `loop()` vacío

Toda la lógica está en la tarea. `loop()` sólo hace `vTaskDelay(500)` para no gastar CPU girando en
vacío. (Otra opción equivalente sería borrar esa tarea con `vTaskDelete(NULL)`.)

---

## 1.5. Resumen de funciones de FreeRTOS usadas

| Función | Dónde | Para qué |
|---|---|---|
| `xTaskCreatePinnedToCore` | Tx y Rx, `setup()` | Crear la tarea principal y fijarla al núcleo 1 |
| `xTaskGetTickCount` | Tx | Tomar el instante inicial del ciclo periódico |
| `vTaskDelayUntil` | Tx | Período exacto de 20 ms |
| `vTaskDelay` | Tx y Rx, `loop()` | Bloquear `loop()` para que no gire en vacío |
| `pdMS_TO_TICKS` | Ambos | Convertir milisegundos a ticks |
| `xQueueCreate` | Rx | Crear el buzón de un elemento |
| `xQueueOverwriteFromISR` | Rx, callback | Dejar el último paquete en el buzón, pisando el anterior |
| `portYIELD_FROM_ISR` | Rx, callback | Pedir cambio de tarea si se despertó una más prioritaria |
| `xQueueReceive` | Rx, tarea | Esperar un paquete con timeout de 20 ms |

---

## 1.6. Preguntas de práctica

**¿Por qué usaron FreeRTOS?**
Ya viene en el ESP32. Nos permite tener una tarea con período fijo de 20 ms y prioridad propia, separada
de la radio y del puerto serie. Con un `loop()` y `millis()` el período depende de todo lo demás.

**¿Qué diferencia hay entre `vTaskDelay` y `vTaskDelayUntil`?**
`vTaskDelay(20)` espera 20 ms desde *ahora*; el período real es 20 ms más el tiempo de trabajo.
`vTaskDelayUntil` despierta en instantes fijos (cada 20 ms desde el inicio); el período no se alarga.

**¿Por qué fijaron las tareas al núcleo 1?**
Porque la pila Wi-Fi y ESP-NOW corren en el núcleo 0. Así la radio no interrumpe nuestro control y
nuestro control no demora a la radio.

**¿Por qué una cola y no una variable global?**
El callback de radio y la tarea de control corren en núcleos distintos, al mismo tiempo. Con una
variable global la tarea podría leer un paquete a medio copiar. La cola copia los 19 bytes de forma
protegida.

**¿Por qué la cola es de un solo elemento?**
Porque sólo importa el último paquete. Con una cola más larga se acumularían paquetes viejos y el retardo
crecería.

**¿Qué pasa si llegan dos paquetes antes de que la tarea lea?**
El segundo pisa al primero (`Overwrite`). Se pierde el viejo, que de todos modos ya no servía.

**¿Qué pasa si no llegan paquetes?**
`xQueueReceive` vence a los 20 ms y la tarea sigue su ciclo: a los 250 ms sin paquetes entra en failsafe
y lleva la mano a Home.

**¿El callback de ESP-NOW es una interrupción?**
No estrictamente: corre en la tarea de Wi-Fi del núcleo 0. Por eso tiene que ser corto: sólo valida y
copia. Usamos la variante `FromISR` de la cola; funciona, pero la canónica sería `xQueueOverwrite`.

**¿Por qué la calibración no necesita un semáforo para la bandera `calibrando`?**
Porque `loop()` y la tarea de transmisión están en el mismo núcleo y la tarea tiene más prioridad:
`loop()` sólo corre cuando la tarea está esperando su próximo ciclo, nunca en medio de un envío. Y es un
`bool`, que se escribe en una sola operación.

**¿Qué pasaría si una tarea de prioridad alta no tuviera ninguna espera?**
Acapararía su núcleo: las tareas de menor prioridad (incluida *Idle*) no correrían nunca y el watchdog
del sistema podría reiniciar la placa.

**¿Cómo saben que 4096 bytes de pila alcanzan?**
Es un valor habitual y el sistema funciona sin errores de pila, pero no lo medimos. Se podría comprobar con
`uxTaskGetStackHighWaterMark()`.
