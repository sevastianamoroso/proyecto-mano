# Aprendizaje: entender el firmware para poder explicarlo

Guía de estudio para la defensa. No repite el detalle de
[`EXPLICACION_CODIGO.md`](../../EXPLICACION_CODIGO.md): explica **los conceptos de base** y **el porqué
de cada decisión** en frases cortas, listas para decir en voz alta. Cuando haga falta el detalle, se
enlaza al documento existente.

## Orden de lectura

| # | Archivo | Qué aprendés |
|---|---|---|
| 1 | [`01_FREERTOS.md`](01_FREERTOS.md) | Qué es un sistema operativo de tiempo real, tareas, prioridades, núcleos, colas y cómo los usamos |
| 2 | [`02_ESPNOW.md`](02_ESPNOW.md) | Qué es ESP-NOW, cómo se envía y recibe un paquete, por qué lo elegimos frente a Wi-Fi o Bluetooth |
| 3 | [`03_CODIGO_PARTE_POR_PARTE.md`](03_CODIGO_PARTE_POR_PARTE.md) | Cada bloque del protocolo, el transmisor y el receptor, con la razón de cada decisión |
| 4 | [`04_PREGUNTAS_DEFENSA.md`](04_PREGUNTAS_DEFENSA.md) | 60 preguntas y respuestas: proyecto, hardware, pruebas, limitaciones, conocimiento complementario y la demo |

Cada archivo termina con **preguntas de práctica** del tipo que puede hacer el jurado, con una respuesta
corta. Conviene taparlas y tratar de responder primero.

## El sistema en 30 segundos (para abrir la explicación)

> El guante tiene un ESP32 que lee cinco potenciómetros (uno por dedo) y una IMU MPU6050 (la muñeca).
> Cada 20 ms convierte esas lecturas en **ángulos de servo ya calculados**, los mete en un paquete de
> 19 bytes con una firma y un checksum, y lo manda por **ESP-NOW** al ESP32 de la mano. La mano valida
> el paquete, lo pasa por una **cola de FreeRTOS** a la tarea de control, y ésta mueve los siete servos
> con un límite de velocidad y una banda muerta. Si dejan de llegar paquetes durante 250 ms, la mano
> vuelve sola a una posición segura.

## Las tres ideas que ordenan todo el código

1. **El guante piensa, la mano obedece.** Toda la calibración del operador vive en el transmisor; el
   receptor sólo recibe ángulos y los ejecuta con seguridad. Cambiar de operador = recalibrar el guante.
2. **Radio y control no se mezclan.** La radio (Wi-Fi/ESP-NOW) corre en el núcleo 0; nuestras tareas
   de control en el núcleo 1. Se comunican por una cola, nunca por variables compartidas.
3. **Nada se mueve de golpe ni sin control.** Límites mecánicos (`constrain`), límite de velocidad,
   banda muerta y failsafe: el receptor nunca confía ciegamente en lo que llega.

## Glosario rápido

| Término | En una frase |
|---|---|
| **RTOS** | Sistema operativo que garantiza *cuándo* se ejecuta cada cosa, no sólo *que* se ejecute |
| **Tarea** (*task*) | Una función con su propio bucle infinito y su propia pila, que el sistema operativo planifica |
| **Scheduler** | La parte de FreeRTOS que decide qué tarea corre en cada momento |
| **Tick** | La "unidad de reloj" del RTOS. En el core Arduino del ESP32 vale 1 ms |
| **Prioridad** | Número que dice qué tarea gana si dos quieren correr a la vez (mayor = gana) |
| **Bloquear** | Una tarea "duerme" esperando algo (tiempo o un dato) sin gastar CPU |
| **Cola** (*queue*) | Canal seguro para pasar datos entre tareas; copia el dato completo |
| **Callback** | Función nuestra que el sistema llama cuando ocurre un evento (llegó un paquete, terminó un envío) |
| **ISR** | Rutina de interrupción: código que corre al instante ante un evento de hardware |
| **Condición de carrera** | Error que aparece cuando dos partes del programa usan el mismo dato a la vez |
| **Peer** | En ESP-NOW, el otro dispositivo con el que hablamos, identificado por su MAC |
| **MAC** | Dirección física única de la placa (6 bytes), como un "DNI" de la radio |
| **ACK** | Acuse de recibo que la radio del receptor devuelve automáticamente |
| **Failsafe** | Comportamiento seguro por defecto cuando algo falla (acá: ir a Home) |
| **Slew-rate** | Límite de velocidad de cambio: cuántos grados puede avanzar un servo por ciclo |
| **Deadband** | Banda muerta: errores menores a cierto umbral se ignoran para evitar vibración |

## Datos verificados (2026-10-26)

Contra el core Arduino-ESP32 2.0.17 y ESP-IDF 4.4.7, que es lo que usa el proyecto:

| Dato | Fuente |
|---|---|
| Tick de 1 ms (`CONFIG_FREERTOS_HZ = 1000`) | `tools/sdk/esp32/sdkconfig` del core |
| `loopTask` con prioridad 1 en el núcleo 1 (`ARDUINO_RUNNING_CORE = 1`), pila de 8192 bytes | `cores/esp32/main.cpp` y `sdkconfig` del core |
| `delay(ms)` = `vTaskDelay(ms / portTICK_PERIOD_MS)` | `cores/esp32/esp32-hal-misc.c` |
| Tarea Wi-Fi fijada al núcleo 0 | `CONFIG_ESP32_WIFI_TASK_PINNED_TO_CORE_0=y` en el `sdkconfig` |
| ESP-NOW: 250 bytes por paquete, 20 *peers* en total | `ESP_NOW_MAX_DATA_LEN` y `ESP_NOW_MAX_TOTAL_PEER_NUM` en `esp_now.h` |
| El callback de recepción corre en la tarea Wi-Fi | Guía de ESP-NOW de ESP-IDF 4.4 |

En ESP-IDF 5.4 o posterior existe ESP-NOW v2, con hasta 1470 bytes por paquete. El core 2.0.17 no la usa.
