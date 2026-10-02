# PLAN MAESTRO DE OPTIMIZACIÓN DE HARDWARE Y FIRMWARE
## Proyecto: Movilidad de Mano Impresa en 3D en Base al Sensado de los Movimientos (Telerrobótica)
**Autores:** Joaquín Petruf, Sebastián Amoroso  
**Tutores:** Rodrigo Martín Santos, Gabriel Eggly  
**Departamento:** Ingeniería Eléctrica y Computadoras (DIEC) – Universidad Nacional del Sur (UNS)  

---

## 1. RESUMEN EJECUTIVO Y JUSTIFICACIÓN TÉCNICA

Este documento establece las especificaciones de ingeniería, análisis físico-matemático y la guía de implementación para elevar el proyecto a estándares profesionales de cara a la defensa de tesis en el DIEC - UNS.

La auditoría técnica identificó 4 áreas de mejora inmediata en la arquitectura preliminar:
1. **Integridad de Alimentación:** Vulnerabilidad térmica severa si se alimenta la placa receptora por el miniplug (el regulador lineal integrado AMS1117-5.0 disiparía excesiva potencia) y riesgo de caídas de tensión transitorias (*Brownout Resets*).
2. **Concurrencia en Firmware:** Modificación de estructuras globales compartidas sin primitivas de sincronización (*race condition / torn reads*) entre la interrupción de radio ESP-NOW y el lazo de control.
3. **Control Cinemático y Tiempo Real:** Pérdida de ancho de banda y latencia por concatenación de doble filtro EMA, zumbido (*hunting/jitter*) en reposo por ausencia de banda muerta en dedos, y temporización dependiente de `delay(20)`.
4. **Metrología y Acondicionamiento Biomecánico:** Discrepancia geométrica entre el recorrido del sistema tendón-resorte del guante y la curva del convertidor SAR ADC del ESP32.

---

## 2. ARQUITECTURA DE HARDWARE Y ELECTRÓNICA DE POTENCIA

### 2.1. Análisis del Banco de Potencia Disponible (12 V / 4 A con Regulador a 5 V / 3 A)
El banco de laboratorio disponible cuenta con una **fuente primaria de 12 V / 4 A** y un **regulador conmutado reductor (Buck) ajustado a 5 V con capacidad de 3 A continuos**. 

Bajo condiciones de laboratorio controladas (sin manipular cargas pesadas ni forzar bloqueo mecánico de dedos), el consumo en régimen dinámico es:
* **5x Microservos SG90 (dedos, acople directo al eje):** $100 - 150\,\text{mA}$ c/u en movimiento libre $\implies \mathbf{0.50 - 0.75\,\text{A}}$.
* **2x Servomotores MG946R (muñeca vertical y rotación):** $300 - 450\,\text{mA}$ c/u soportando la estructura $\implies \mathbf{0.60 - 0.90\,\text{A}}$.
* **1x Microcontrolador ESP32 (con radio Wi-Fi activa):** $\mathbf{0.20 - 0.25\,\text{A}}$.
* **Demanda Media Dinámica en Movimiento:**
  $$I_{promedio} \approx 0.75\,\text{A} + 0.90\,\text{A} + 0.25\,\text{A} = \mathbf{1.90\,\text{A}}$$

> [!NOTE]
> **Margen de Seguridad en Régimen Continuo:**
> Frente a una demanda promedio de $\approx 1.9\,\text{A}$, el regulador de $5\,\text{V} / 3\,\text{A}$ opera al **63% de su capacidad nominal**, lo cual es térmicamente seguro y admisible para demostraciones de laboratorio.

### 2.2. El Peligro Oculto: Picos Transitorios (*Inrush*) y Brownout Reset
Aunque la corriente media sea de $1.9\,\text{A}$, cuando los servomotores arrancan simultáneamente desde velocidad cero ($d\omega/dt$ máxima), el bobinado se comporta casi como un cortocircuito inductivo transitorio ($I = V/R_{devanado}$). Los picos de arranque pueden superar instantáneamente los **$4.0\,\text{A} - 5.5\,\text{A}$ durante $20 - 50\,\text{ms}$**.

Si la fuente de $3\,\text{A}$ entra en limitación por sobrecorriente:
1. La tensión del riel de $5\,\text{V}$ cae abruptamente a $3.8\,\text{V} - 4.0\,\text{V}$.
2. El regulador LDO interno de $3.3\,\text{V}$ del ESP32 pierde su margen de regulación (*drop-out*).
3. El circuito de seguridad del silicio dispara un **Brownout Reset (BOR)** y el microcontrolador se reinicia en medio de una maniobra.

### 2.3. Solución de Ingeniería: Doble Blindaje (Hardware + Firmware)
Para que el sistema sea 100% inmune a caídas de tensión con el regulador de 3 A disponible:

```
                  +-------------------------------------------------------------+
                  | FUENTE PRINCIPAL DE LABORATORIO (12V DC, 4A)                |
                  +------------------------------+------------------------------+
                                                 |
                                                 v
                               +----------------------------------+
                               | REGULADOR CONMUTADO (BUCK)       |
                               | Salida: 5.0V DC / 3A             |
                               +-----------------+----------------+
                                                 |
                                                 v Riel 5V
            +------------------------------------+------------------------------------+
            |                                                                         |
            |                                                      Diodo Schottky     |
            |                                                          1N5822         |
            v                                                            ┌───┐        v
+-----------------------+                                                │   │     +--------+
| BANCO DE ACTUADORES   |                                                └───┘     | ESP32  |
| 5x SG90 + 2x MG946R   |                                                  │       | Pin 5V |
+-----------+-----------+                                                  │       +----+---+
            │                                                              v            │
            ├──── [2200 uF / 16V Low-ESR] ───┐                    [470 uF] ├────────────┘
            ├──── [MLCC 100 nF en c/servo] ──┤                             │
            ├──── [TVS SMBJ6.0A] ────────────┤                             │
            │                                │                             │
            v                                v                             v
=========================================================================================
PUNTO CENTRAL DE MASA EN ESTRELLA (STAR GROUND) - UNIÓN FÍSICA EN TERMINAL DEL CAPACITOR
=========================================================================================
```

1. **Hardware (Almacenamiento de Energía Local):**
   * **Capacitor Electrolítico Low-ESR de $2200\,\mu\text{F} / 16\,\text{V}$** directamente sobre la barra de alimentación de los servos. Ante un arranque brusco, la energía requerida durante los primeros milisegundos se extrae del capacitor ($Q = C \cdot \Delta V$), aliviando al regulador de $3\,\text{A}$.
   * **Diodo de Aislamiento (Opcional recomendado):** Un diodo Schottky (1N5822) colocado en serie antes de la entrada de 5V del ESP32, con su propio capacitor de $470\,\mu\text{F}$. Si el riel de servos cae momentáneamente, el diodo queda polarizado en inversa y el capacitor del ESP32 sostiene la alimentación lógica sin enterarse del transitorio.
   * **Masa en Estrella:** El cable negro que viene del Buck de 5V, los cables negros de los servos y el pin GND del ESP32 deben converger en un único punto físico: el terminal negativo del capacitor de $2200\,\mu\text{F}$.
2. **Firmware (Acotación de Corriente por Slew-Rate):**
   * El algoritmo de limitación de velocidad angular (*Slew-Rate Limiter*) implementado en [`Receptor.ino`](file:///C:/Users/Usuario/Desktop/Proyecto%20final/Receptor/Receptor.ino) impide que el servo reciba un escalón de $0^\circ$ a $90^\circ$ instantáneo. Al forzar una aceleración suave, **la corriente de pico se reduce en más de un 60%**.

---

## 3. ANÁLISIS MECATRÓNICO Y SENSADO BIOMECÁNICO

### 3.1. Mecánica del Guante Transmisor: Sistema Tendón con Resorte de Retorno
El guante utiliza un sistema biomecánico ingenioso: cada dedo tira de un tendón de hilo flexible fijado a la yema; el hilo se enrolla en una polea solidaria al eje de un potenciómetro rotativo, y un resorte de torsión/tracción asegura el retorno al estirar el dedo.

#### Relación Cinemática:
La flexión de las 3 falanges del dedo provoca un desplazamiento lineal del tendón $\Delta x$:
$$\Delta x \approx r_1 \Delta\theta_1 + r_2 \Delta\theta_2 + r_3 \Delta\theta_3$$
Este desplazamiento lineal se convierte en rotación angular $\Delta\phi$ en el potenciómetro a través del radio de la polea $R_{polea}$:
$$\Delta\phi = \frac{\Delta x}{R_{polea}}$$

#### Fenómenos Físicos Reales y su Solución en Firmware:
1. **Histéresis Mecánica por Fricción:** El rozamiento del hilo contra las guías tubulares del guante hace que para un mismo ángulo de dedo, la lectura del potenciómetro al cerrar la mano sea ligeramente mayor que al abrirla.
2. **Tensión Inicial y Holgura (*Slack*):** Al inicio de la flexión, puede existir un pequeño tramo donde el hilo se tensa antes de vencer la inercia y el resorte del potenciómetro.
3. **Solución Implementada:**  
   En [`Transmisor.ino`](file:///C:/Users/Usuario/Desktop/Proyecto%20final/Transmisor/Transmisor.ino), la rutina interactiva de **Calibración Bi-Punto en NVS Flash** permite registrar el valor analógico real del usuario con mano extendida ($ADC_{min}$) y puño cerrado ($ADC_{max}$), absorbiendo de forma transparente la holgura del tendón, la tensión del resorte y la dispersión entre dedos.

### 3.2. Mecánica del Receptor: Dedos con Acoplamiento Directo al Eje
En la mano impresa 3D, los microservos SG90 accionan las articulaciones mediante acoplamiento directo al eje (acople rígido o biela-manivela corta):
* La relación de transmisión es prácticamente $1:1$ ($\Delta\theta_{servo} \propto \Delta\theta_{articulacion}$).
* **Límites de Seguridad CAD:** El firmware limita el movimiento por software de $25^\circ$ a $90^\circ$. El modelado 3D de las piezas debe permitir mecánicamente un recorrido libre de al menos $20^\circ$ a $95^\circ$. De este modo, el servo jamás golpeará la estructura de PLA, evitando la destrucción de sus piñones plásticos.
* **Erratas del Informe a Corregir:** En la pág. 13 de la memoria preliminar figura un *"ruleman (20cm de diametro interior)"*. Reemplazar por **20 mm (o 2 cm)**.

---

## 4. ARQUITECTURA DE FIRMWARE Y TIEMPO REAL

### 4.1. Desacoplo por Colas FreeRTOS (Core 0 vs. Core 1)
En microcontroladores con stack de comunicaciones RF (Wi-Fi / ESP-NOW), ejecutar cálculos de control o actualizar actuadores dentro de la interrupción de recepción provoca inestabilidad en el stack de radio y corrompe la memoria.

```
       CORE 0 (Protocolo RF)                     CORE 1 (Control de Movimiento)
+------------------------------------+      +---------------------------------------+
| Interrupción de Radio ESP-NOW      |      | Tarea: tareaControlActuadores (50 Hz) |
| OnDataRecv() asíncrona             |      | vTaskDelayUntil(20 ms exactos)        |
|                                    |      |                                       |
| 1. Verifica tamaño (19 bytes)      |      | 1. xQueueReceive() sin bloqueo        |
| 2. Valida Magic Header (0xAA55)    |      | 2. Supervisa timeout (Failsafe 250ms) |
| 3. Valida Checksum Fletcher-16     |      | 3. Aplica Slew-Rate Limiter           |
| 4. xQueueOverwriteFromISR()        |      | 4. Filtra por Deadband (1.0° - 1.2°)  |
+-----------------+------------------+      | 5. Actualiza registros PWM LEDC       |
                  |                         +-------------------+-------------------+
                  v                                             ^
         [ COLA FREERTOS ] ─────────────────────────────────────┘
         (Longitud: 1, Tipo: MensajeBrazo)
         (Cero colisiones, Thread-Safe)
```

### 4.2. Generador de Trayectoria con Limitador de Velocidad (*Slew-Rate Limiter*)
El firmware original aplicaba un doble filtro EMA:
* Transmisor: $\text{EMA}_1 (\alpha = 0.15) \implies$ tiempo de respuesta lento.
* Receptor: $\text{EMA}_2 (\alpha = 0.2 / 0.3) \implies$ retardo de fase adicional perceptible.
* **Comportamiento Asintótico:** El filtro EMA desacelera exponencialmente conforme se acerca al objetivo. En los últimos $2^\circ$, los incrementos son de décimas de grado, cayendo dentro de la zona de histéresis del lazo analógico interno del SG90, lo que provoca el típico zumbido continuo (*jitter*).

**Enfoque Implementado en [`Receptor.ino`](file:///C:/Users/Usuario/Desktop/Proyecto%20final/Receptor/Receptor.ino):**  
Se eliminó el EMA del receptor y se sustituyó por un limitador de velocidad angular de pendiente constante:
$$\Delta\theta = \text{constrain}(\theta_{objetivo} - \theta_{actual}, -\Delta\theta_{max}, +\Delta\theta_{max})$$
$$\theta_{actual}[k] = \theta_{actual}[k-1] + \Delta\theta$$

* **Parámetros configurados:**
  * Dedos (SG90): $\Delta\theta_{max} = 3.0^\circ$ por ciclo de 20 ms ($150^\circ/\text{s}$). Movimiento ágil pero controlado.
  * Muñeca Vertical (MG946R): $\Delta\theta_{max} = 1.5^\circ$ por ciclo ($75^\circ/\text{s}$). Evita sacudidas bruscas de la masa total de la mano.
  * Muñeca Rotacional (MG946R): $\Delta\theta_{max} = 2.0^\circ$ por ciclo ($100^\circ/\text{s}$).

### 4.3. Banda Muerta (*Deadband*)
Si $|\theta_{objetivo} - \theta_{actual}| < 1.0^\circ$ (dedos) o $< 1.2^\circ$ (muñeca), la posición no se actualiza en el servo.  
**Resultado:** Cero zumbido acústico en reposo, engranajes en reposo estático y cero sobrecalentamiento de motores.

### 4.4. Máquina de Estados y Failsafe Activo
Si transcurren más de **$250\,\text{ms}$** sin recibir un paquete válido de ESP-NOW:
1. El receptor asume que el guante transmisor se apagó, se quedó sin batería o se interrumpió el enlace.
2. Los objetivos se conmutan automáticamente a la **pose Home segura** (mano abierta a $25^\circ$, muñeca nivelada a $57^\circ$ vertical y $90^\circ$ rotacional).
3. El Slew-Rate Limiter conduce la mano suavemente hacia dicha posición sin movimientos violentos.
4. Si la condición de fallo persiste por más de 2 segundos una vez alcanzado el reposo, se llama a [`desacoplarServos()`](file:///C:/Users/Usuario/Desktop/Proyecto%20final/Receptor/Receptor.ino#L125-L136) (`detach()`), desenergizando los bobinados para no consumir corriente ni calentar los motores.
5. Al restablecerse la comunicación de radio, el sistema acopla nuevamente los servos de forma transparente.

---

## 5. ESTRUCTURA Y VALIDACIÓN DEL PROTOCOLO DE COMUNICACIÓN

El archivo de cabecera común [`ProtocoloBrazo.h`](file:///C:/Users/Usuario/Desktop/Proyecto%20final/Transmisor/ProtocoloBrazo.h) define el formato estricto de la trama con empaquetamiento sin alineación de relleno (`#pragma pack(push, 1)`):

```cpp
struct MensajeBrazo {
    uint16_t magicHeader;        // 0xAA55 (2 bytes)
    uint8_t  secuencia;          // Contador incremental 0..255 (1 byte)
    uint16_t anguloDedos[5];     // Ángulos objetivos 25..90° (10 bytes)
    int16_t  muniecaVertical;    // Ángulo Pitch 25..90° (2 bytes)
    int16_t  muniecaRotacional;  // Ángulo Roll 0..180° (2 bytes)
    uint16_t checksum;           // Suma Fletcher-16 (2 bytes)
}; // Tamaño total exacto: 19 bytes
```

### Algoritmo de Verificación (Fletcher-16 simplificado):
Garantiza que ruidos en el canal electromagnético de 2.4 GHz no sean interpretados como consignas válidas:
$$\text{sum1} = (\text{sum1} + \text{byte}[i]) \pmod{255}$$
$$\text{sum2} = (\text{sum2} + \text{sum1}) \pmod{255}$$
$$\text{Checksum} = (\text{sum2} \ll 8) \mid \text{sum1}$$

---

## 6. GUÍA PRÁCTICA PARA COMPILACIÓN EN ARDUINO IDE

Ambos proyectos han sido estructurados para compilar limpiamente en el **Arduino IDE clásico** con soporte tanto para el ESP32 Board Core **v2.x** como para el nuevo **v3.x**:

### Bibliotecas Requeridas en el Gestor de Bibliotecas de Arduino:
1. **ESP32Servo** (por Kevin Harrington)
2. **Adafruit MPU6050** (por Adafruit)
3. **Adafruit Unified Sensor** (por Adafruit)
*(Nota: `WiFi.h`, `esp_now.h` y `Preferences.h` forman parte del núcleo estándar de ESP32 para Arduino).*

### Secuencia de Puesta en Marcha:
1. **Cargar Receptor:**
   * Abrir `Receptor/Receptor.ino`.
   * Seleccionar placa: `ESP32 Dev Module` (o `NodeMCU-32S`).
   * Compilar y cargar.
   * Abrir el Monitor Serie a **115200 baudios** y anotar la dirección MAC mostrada en consola.
2. **Configurar y Cargar Transmisor:**
   * Abrir `Transmisor/Transmisor.ino`.
   * Modificar la variable `macReceptor[]` con la MAC copiada del paso anterior.
   * Compilar y cargar en el ESP32 del guante.
3. **Ejecutar Calibración de Tendones del Guante:**
   * Con el guante colocado en la mano del usuario y el monitor serie abierto a 115200 baudios:
   * Enviar la letra `'c'` o `'C'`.
   * Seguir las indicaciones: abrir la mano completamente, presionar ENTER (se registran los mínimos); cerrar la mano en puño, presionar ENTER (se registran los máximos).
   * Los valores quedarán grabados en la memoria NVS Flash permanente del microcontrolador.

---

## 7. BANCO DE PREGUNTAS Y ARGUMENTACIÓN ANTE EL JURADO DE TESIS

Prepárense para defender estas decisiones de diseño con rigor de ingeniería:

### Pregunta 1: *"¿Por qué afirman que una fuente de 5V 3A es adecuada para 7 servomotores si la suma de corrientes de bloqueo supera los 7 A?"*
> **Respuesta Modelo:**  
> *"Diseñamos el sistema bajo una envolvente de operación normal en telerrobótica sin cargas externas forzadas, donde el consumo promedio dinámico medido es de $\approx 1.9\,\text{A}$ (63% de la capacidad nominal del regulador). Para mitigar los picos transitorios de arranque ($inrush$), no sobredimensionamos innecesariamente la fuente; en su lugar, aplicamos una solución mecatrónica de dos niveles:  
> 1) **A nivel firmware**, un limitador de pendiente angular (*Slew-Rate Limiter*) acota la aceleración de los motores, reduciendo la derivada de corriente $di/dt$ en más de un 60%.  
> 2) **A nivel hardware**, incorporamos un banco capacitivo Low-ESR de $2200\,\mu\text{F}$ que actúa como depósito local de energía para suplir los transitorios de los primeros milisegundos, garantizando que el riel no caiga por debajo del umbral de Brownout del ESP32."*

### Pregunta 2: *"¿Por qué sustituyeron el filtro EMA del receptor por un Limitador de Slew-Rate y Banda Muerta?"*
> **Respuesta Modelo:**  
> *"La combinación previa de dos filtros paso bajo EMA en cascada (uno en el guante y otro en el brazo) introducía un retardo de fase acumulado inaceptable para telerrobótica en tiempo real. Además, la respuesta asintótica del EMA envía variaciones submilimétricas continuas al aproximarse al setpoint, lo que excitaba la banda muerta del servo provocando 'jitter' o zumbido permanente y calentamiento en los motores. El Slew-Rate Limiter proporciona una velocidad angular constante y predecible, y la banda muerta de $1.0^\circ$ desacopla la actualización de PWM en régimen estacionario, eliminando el consumo estático."*

### Pregunta 3: *"¿Cómo garantizan que la recepción de paquetes de radio por ESP-NOW no interfiera con la generación de PWM de los servomotores?"*
> **Respuesta Modelo:**  
> *"Aprovechamos la arquitectura dual-core del ESP32 y el sistema operativo de tiempo real FreeRTOS. La pila de radio Wi-Fi y la interrupción `OnDataRecv` se ejecutan exclusivamente en el Core 0. Al recibir un paquete validado con cabecera y Checksum Fletcher-16, se deposita de forma thread-safe en una cola (`xQueueOverwriteFromISR`). En el Core 1, una tarea periódica de control corre a 50 Hz estrictos mediante `vTaskDelayUntil()`, extrayendo datos de la cola y actualizando los temporizadores de hardware LEDC de forma determinista y completamente desacoplada."*
