# Explicación del Funcionamiento del Sistema de Telerrobótica

**Proyecto:** Movilidad de Mano Impresa en 3D en Base al Sensado de los Movimientos (Telerrobótica)  
**Institución:** Departamento de Ingeniería Eléctrica y Computadoras (DIEC) – Universidad Nacional del Sur (UNS)  
**Autores:** Joaquín Petruf, Sebastián Amoroso  

---

## 1. Visión General del Sistema

El sistema permite replicar en tiempo real los movimientos de una mano y muñeca humanas sobre una mano robótica antropomórfica impresa en 3D. 

El hardware está dividido en dos nodos principales equipados con microcontroladores **ESP32**:
1. **Guante Transmisor:** Lee la flexión de los dedos y la orientación de la muñeca del operador humano, procesa y filtra los datos, y los envía de forma inalámbrica.
2. **Brazo Receptor:** Recibe los comandos por radio, valida su integridad, genera trayectorias de movimiento suaves para proteger la estructura mecánica y comanda los servomotores.
3. **Enlace Inalámbrico (ESP-NOW):** Protocolo de comunicación directa por radiofrecuencia (2.4 GHz) desarrollado por Espressif, que opera sin requerir un router Wi-Fi y ofrece una latencia inferior a 5 milisegundos.

```
 [ GUANTE TRANSMISOR ]                                  [ BRAZO RECEPTOR ]
 5 Potenciómetros (Dedos)                               5 Servos SG90 (Dedos)
        +                                                      +
 Sensor MPU6050 (Muñeca)                               2 Servos MG946R (Muñeca)
        │                                                      ▲
        ▼                                                      │
┌─────────────────────────┐     ESP-NOW 2.4 GHz       ┌─────────────────────────┐
│ ESP32 Transmisor        │ ════════════════════════> │ ESP32 Receptor          │
│ • Lectura y Filtrado    │    (Tasa: 50 Hz / 20 ms)  │ • Validación y Failsafe │
│ • Filtro de Kalman      │                           │ • Suavizado (Slew-Rate) │
│ • Empaquetado binario   │                           │ • Control PWM de Servos │
└─────────────────────────┘                           └─────────────────────────┘
```

---

## 2. El Protocolo de Comunicación (`ProtocoloBrazo.h`)

Para garantizar que los datos viajen sin retrasos y sin riesgo de corromperse en el aire, se definió una estructura de datos binaria compartida empaquetada a nivel de byte (`#pragma pack(push, 1)`):

### Estructura del Paquete (`MensajeBrazo`)
* **`magicHeader` (`0xAA55`):** Identificador exclusivo de inicio de trama. Si el receptor recibe un paquete que no comienza con esta firma, lo descarta inmediatamente.
* **`secuencia`:** Contador secuencial de 0 a 255. Permite auditar la calidad del enlace e identificar pérdidas de paquetes.
* **`anguloDedos[5]`:** Arreglo con la posición angular objetivo para cada dedo (Pulgar, Índice, Medio, Anular y Meñique) en el rango calibrado de **25° a 90°**.
* **`muniecaVertical` (Pitch):** Ángulo de elevación/depresión vertical de la muñeca (**25° a 90°**).
* **`muniecaRotacional` (Roll):** Ángulo de giro/pronosupinación de la muñeca (**0° a 180°**).
* **`checksum` (Fletcher-16):** Suma de verificación matemática calculada sobre todos los bytes anteriores. Si una interferencia electromagnética altera aunque sea un solo bit, el cálculo no coincide y el paquete se descarta de forma segura.

---

## 3. Funcionamiento del Guante Transmisor (`Transmisor.ino`)

El transmisor ejecuta su lazo principal a una frecuencia de **50 Hz** (un ciclo de sensado y envío cada 20 milisegundos) mediante una tarea de **FreeRTOS** (`TaskTxBrazo`) anclada en el **Núcleo 1** del ESP32, dejando el Núcleo 0 libre para gestionar la pila Wi-Fi de radiofrecuencia.

### Paso a Paso del Transmisor:

### 1. Inicialización Segura y Calibración (`setup`)
* **Carga de Calibración desde Flash (NVS):** Lee los rangos mínimo (mano abierta) y máximo (puño cerrado) de cada potenciómetro guardados previamente en memoria no volátil (`Preferences`).
* **Calibración Interactiva:** Permite presionar la tecla `'c'` en el monitor serial para registrar y almacenar de forma permanente los límites de cada dedo sin tener que recompilar el firmware.
* **Recuperación del Bus I2C (`rutinaRecuperacionI2C`):** Genera 9 pulsos de reloj manuales en la línea SCL para destrabar el sensor de la muñeca en caso de que haya quedado bloqueado tras un reinicio intempestivo.
* **Configuración del MPU6050:** Ajusta el acelerómetro a ±2G, el giróscopo a ±500°/s y activa el filtro paso-bajo digital interno (DLPF a 21 Hz) para amortiguar vibraciones mecánicas.
* **Configuración de ESP-NOW:** Registra la dirección física (MAC) del receptor y desactiva el modo de ahorro de energía Wi-Fi (`WiFi.setSleep(false)`) para anular demoras de radio.

### 2. Adquisición y Filtrado de los Dedos
* Cada dedo cuenta con un hilo de retorno conectado a un potenciómetro rotativo.
* **Sobremuestreo (16x):** Para mitigar el ruido de cuantización y alinealidades del conversor analógico-digital (ADC) del ESP32, el sistema toma 16 lecturas sucesivas con micro-retardos y calcula el promedio matemático.
* **Mapeo Angular:** Convierte la lectura de tensión promedio a un ángulo entre **25° (extendido)** y **90° (flexionado)** en función de los límites calibrados en NVS.

### 3. Fusión Sensorial de Muñeca con Filtro de Kalman (`KalmanFilter.h`)
* El sensor MPU6050 combina dos tecnologías complementarias:
  - **Acelerómetro:** Proporciona un ángulo de inclinación absoluto y estable a largo plazo mediante funciones trigonométricas (`atan2`), pero es altamente sensible a aceleraciones parásitas y vibraciones.
  - **Giróscopo:** Mide velocidad angular con respuesta instantánea y sin ruido, pero sufre de deriva (*drift*), acumulando error al integrarse en el tiempo.
* **Filtro de Kalman:**
  1. *Predicción:* Proyecta el nuevo ángulo en base a la velocidad angular del giróscopo.
  2. *Corrección:* Compara la predicción con la referencia gravitatoria del acelerómetro y ajusta la estimación según la matriz de covarianza de error.
  3. *Resultado:* Ángulos de **Pitch** y **Roll** estables, limpios de ruido y con respuesta inmediata en tiempo real.
* **Protección de Singularidad Gimbal:** Si la inclinación vertical supera ±80°, el cálculo de rotación bloquea temporalmente la actualización para prevenir indeterminaciones matemáticas (bloqueo de cardán).

### 4. Empaquetado y Transmisión
* Se ensamblan los 5 dedos y los 2 ejes de la muñeca en la estructura `paqueteSalida`.
* Se genera el número de secuencia correlativo y se calcula el Checksum Fletcher-16.
* Se despacha la trama por aire al receptor mediante `esp_now_send()`.
* Cada 10 ciclos (5 Hz), se imprime telemetría en el monitor serie sin interferir con los tiempos de control.

---

## 4. Funcionamiento del Brazo Receptor (`Receptor.ino`)

El receptor es el nodo que interpreta las órdenes inalámbricas y coordina el movimiento de los 7 servomotores:
* **5x Microservos SG90:** Accionamiento directo de los dedos.
* **2x Servos de alto torque MG946R:** Elevación e inclinación (Pitch) y rotación (Roll) de la muñeca.

### Paso a Paso del Receptor:

### 1. Recepción y Desacoplamiento por Colas (`OnDataRecv`)
* Cuando arriba un paquete de radio, se activa una rutina de interrupción (ISR) gestionada por el Núcleo 0.
* Se verifica la cabecera `0xAA55` y la validez del Checksum.
* **Cola de Mensajes FreeRTOS (`xQueueOverwriteFromISR`):** En lugar de manipular los servos dentro de la interrupción (lo que bloquearía la CPU y causaría inestabilidad), el paquete válido se coloca en una cola protegida de tamaño 1. Esto elimina por completo problemas de concurrencia y carreras críticas (*race conditions*).

### 2. Tarea de Control Periódica a 50 Hz (`tareaControlActuadores`)
En el Núcleo 1, una tarea periódica despierta cada 20 ms mediante `vTaskDelayUntil()` y ejecuta la secuencia de control:

1. **Lectura de la Cola:** Si hay un nuevo paquete disponible, actualiza las posiciones objetivo (`posObjetivo`) de cada servomotor y renueva la marca de tiempo de actividad.
2. **Máquina de Estados y Protección Failsafe:**
   - Si no se reciben paquetes válidos durante más de **250 ms** (pérdida de enlace por distancia, batería baja o apagado del guante), el sistema conmuta al estado `ESTADO_FAILSAFE_SIN_SENAL`.
   - En este modo, se fuerzan todas las referencias a la pose neutral de seguridad (**Home**: dedos a 25°, muñeca centrada a 57° vertical y 90° rotacional).
   - Si el sistema permanece en Failsafe y ya alcanzó la posición Home durante 2 segundos consecutivos, se ejecuta `desacoplarServos()`, cortando los pulsos PWM para liberar los motores, evitar calentamiento estático y prolongar la vida útil del mecanismo.
3. **Limitador de Velocidad Angular (*Slew-Rate Limiter*):**
   - Si se enviara un escalón directo de 0° a 90°, el servo intentaría moverse a su velocidad máxima física, provocando sobreesfuerzos mecánicos sobre los engranajes y piezas impresas en 3D, además de picos de corriente (*inrush current*) capaces de provocar un *Brownout Reset* en la placa.
   - El algoritmo limita la variación máxima de ángulo permitida por cada ciclo de 20 ms:
     - **Dedos (SG90):** Máximo **3.0° por ciclo** (150°/segundo).
     - **Muñeca Vertical (MG946R):** Máximo **1.5° por ciclo** (75°/segundo), diseñado para controlar la inercia del brazo con carga.
     - **Muñeca Rotacional (MG946R):** Máximo **2.0° por ciclo** (100°/segundo).
4. **Supresión de Zumbido por Banda Muerta (*Deadband*):**
   - Los servomotores suelen oscilar y zumbar (*jitter / hunting*) intentando corregir variaciones microscópicas de fracción de grado en reposo.
   - El algoritmo ignora cualquier error de posición inferior a **1.0° en dedos** y **1.2° en muñeca**. Si el operador mantiene la mano quieta, el comando PWM permanece estático y los servos permanecen en silencio absoluto.
5. **Generación de PWM:** Convierte los ángulos calculados en comandos de pulsos mediante el periférico LEDC de hardware del ESP32 a través de la librería `ESP32Servo`.

---

## 5. Cronología de un Movimiento (En Milisegundos)

```
 Instante   Acción Realizada
─────────  ──────────────────────────────────────────────────────────────────────────
 t = 0 ms   El usuario dobla el dedo índice en el guante.
 t + 1 ms   El hilo mueve la polea del potenciómetro variando su resistencia.
 t + 2 ms   El ESP32 toma 16 muestras promediadas en el ADC (sobremuestreo).
 t + 3 ms   El Filtro de Kalman calcula la orientación limpia de la muñeca (Pitch/Roll).
 t + 4 ms   Se empaqueta el struct `MensajeBrazo`, se sella con Fletcher-16 y se envía.
 t + 6 ms   El ESP-NOW del receptor captura el paquete en Core 0 y lo valida en memoria.
 t + 7 ms   El paquete entra a la cola FreeRTOS (`xQueueOverwriteFromISR`).
 t + 20 ms  La tarea del Core 1 toma el dato en su ciclo determinista de 50 Hz.
 t + 21 ms  El limitador Slew-Rate aplica una rampa de avance suave hacia el nuevo ángulo.
 t + 22 ms  El periférico PWM actualiza el pulso del servo.
 t + 40 ms  El servomotor replica el movimiento de forma progresiva, firme y sin sacudidas.
```

---

## 6. Parámetros Técnicos Resumen

| Parámetro | Valor Configurado | Justificación Técnica |
|---|---|---|
| **Frecuencia de Lazo de Control** | 50 Hz (20 ms) | Coincide con la tasa estándar de servos PWM y la cinemática de la mano humana. |
| **Protocolo Inalámbrico** | ESP-NOW (2.4 GHz) | Menor a 5 ms de latencia; sin necesidad de routers ni infraestructura de red. |
| **Algoritmo de Fusión Inercial** | Filtro de Kalman 1D | Elimina ruido de aceleración y deriva de giróscopo sin introducir retardo de fase. |
| **Filtrado Analógico (Dedos)** | Sobremuestreo 16x | Aumenta la resolución efectiva y atenúa el ruido térmico del ADC del ESP32. |
| **Tiempo Límite Failsafe** | 250 ms | Detección rápida de corte de radio para proteger la mano robótica antes de caídas. |
| **Pose de Reposo (Home)** | Dedos: 25°, Muñeca: 57° / 90° | Posición geométricamente relajada sin tensión en tendones mecánicos. |
| **Banda Muerta (Deadband)** | 1.0° (Dedos) / 1.2° (Muñeca) | Elimina el zumbido estático de los servos y reduce drásticamente el consumo térmico. |
| **Límites de Velocidad (Slew-Rate)**| SG90: 150°/s \| MG946R: 75°/s y 100°/s | Suprime picos de corriente inductiva y preserva la integridad de las piezas 3D. |
