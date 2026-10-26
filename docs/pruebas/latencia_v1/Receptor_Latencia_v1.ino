/**
 * @file Receptor_Latencia.ino
 * @brief Versión de PRUEBA DE LATENCIA del Receptor
 * 
 * CAMBIOS respecto al Receptor.ino original:
 *  - Se agrega PIN_SYNC_LATENCIA (GPIO 4) como entrada con interrupción.
 *  - Una ISR captura el instante (micros) del flanco de subida del pulso
 *    emitido por el transmisor.
 *  - Después de escribir TODOS los servos en la tarea de control, se calcula
 *    y reporta la latencia end-to-end (pulso TX → escritura servos RX).
 * 
 * CONEXIÓN FÍSICA REQUERIDA:
 *    TX GPIO 2  ──────────────  RX GPIO 4
 *    TX GND     ──────────────  RX GND
 * 
 * QUÉ MIDE:
 *    Latencia total = t(escritura_servos) − t(flanco_subida_pin_sync)
 *    Esto incluye:
 *      - Tiempo de transmisión RF (ESP-NOW)
 *      - Tiempo de recepción y callback
 *      - Tiempo de encolado en FreeRTOS
 *      - Tiempo de procesamiento (validación, slew-rate, deadband)
 *      - Tiempo de escritura PWM a los 7 servos
 * 
 * TODA la lógica de recepción, FSM, slew-rate, deadband y failsafe
 * permanece IDÉNTICA al firmware original.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_idf_version.h>
#include <ESP32Servo.h>

#include "ProtocoloBrazo.h"

// ==========================================
// PIN DE SINCRONIZACIÓN PARA MEDICIÓN DE LATENCIA
// ==========================================
const int PIN_SYNC_LATENCIA = 4;  // GPIO 4 del receptor ← conectar desde GPIO 2 del transmisor

// Variables de medición de latencia (accedidas desde ISR y tarea)
volatile uint32_t timestampSyncMicros = 0;   // Instante del flanco de subida capturado por ISR
volatile bool     syncPulsoRecibido   = false; // Flag: hay un timestamp válido pendiente

// ==========================================
// ESTADÍSTICAS DE LATENCIA
// ==========================================
// Acumuladores para cálculo de promedio y detección de máximo/mínimo
uint32_t latenciaMinUs   = UINT32_MAX;
uint32_t latenciaMaxUs   = 0;
uint64_t latenciaSumaUs  = 0;
uint32_t latenciaContador = 0;

// ==========================================
// ISR: Captura del pulso de sincronización
// ==========================================
void IRAM_ATTR isrSyncLatencia() {
    timestampSyncMicros = micros();
    syncPulsoRecibido = true;
}

// ==========================================
// CONFIGURACIÓN DE PINES DE SERVOMOTORES
// ==========================================
const int PIN_SERVO_PULGAR        = 25;
const int PIN_SERVO_INDICE        = 26;
const int PIN_SERVO_MEDIO         = 27;
const int PIN_SERVO_ANULAR        = 23; // Antes GPIO 12 (strapping): el dedo se movía con dificultad
const int PIN_SERVO_MENIQUE       = 13;
const int PIN_SERVO_MUNIECA_VERT  = 32;
const int PIN_SERVO_MUNIECA_ROT   = 33;

const int PINES_SERVOS_DEDOS[TOTAL_DEDOS] = {
    PIN_SERVO_PULGAR, PIN_SERVO_INDICE, PIN_SERVO_MEDIO,
    PIN_SERVO_ANULAR, PIN_SERVO_MENIQUE
};

// ==========================================
// PARÁMETROS CINEMÁTICOS Y DE CONTROL
// ==========================================
// Período de actualización del lazo de actuación (50 Hz = 20 ms)
const TickType_t PERIODO_CONTROL_MS = 20;

// Timeout para detección de pérdida de enlace (ms)
const uint32_t TIMEOUT_FAILSAFE_MS = 250;

// Banda muerta angular para supresión de jitter/hunting (grados)
const float DEADBAND_DEDOS   = 1.0f;
const float DEADBAND_MUNIECA = 1.2f;

// Velocidad angular máxima permitida por ciclo (Slew-Rate Limiting a 50 Hz)
// SG90 (dedos): 8.0° / 20ms = 400°/segundo (el SG90 sin carga llega a ~600°/s)
const float VEL_MAX_DEDOS = 8.0f;
// MG946R (muñeca vertical - mayor inercia y carga): 4.0° / 20ms = 200°/segundo
const float VEL_MAX_MUNIECA_VERT = 6.0f;
// MG946R (muñeca rotacional): 5.0° / 20ms = 250°/segundo
const float VEL_MAX_MUNIECA_ROT = 10.0f;

// Sentido de montaje de cada servo de dedo (pulgar..meñique).
// true = el servo cierra el dedo al BAJAR el ángulo: se espeja dentro de 25..90°.
// Ajustar por dedo en la prueba C3: con la mano del guante abierta, el dedo robot debe quedar abierto.
const bool INVERTIR_DEDO[TOTAL_DEDOS] = {true, true, true, true, true};

// Posiciones neutras de reposo seguro (Home)
const float HOME_DEDOS        = 25.0f; // Mano abierta relajada
const float HOME_MUNIECA_VERT = 57.0f; // Posición media horizontal
const float HOME_MUNIECA_ROT  = 90.0f; // Muñeca en ángulo neutro

// Límites mecánicos absolutos (protección de la mano 3D ante paquetes fuera de rango)
const float LIM_DEDOS_MIN = 25.0f,        LIM_DEDOS_MAX = 90.0f;
const float LIM_MUNIECA_VERT_MIN = 25.0f, LIM_MUNIECA_VERT_MAX = 90.0f;
const float LIM_MUNIECA_ROT_MIN = 0.0f,   LIM_MUNIECA_ROT_MAX = 180.0f;

// Ángulo lógico del dedo (25 = abierto, 90 = cerrado) -> ángulo físico del servo
static inline int anguloServoDedo(int i, float pos) {
    return (int)round(INVERTIR_DEDO[i] ? (LIM_DEDOS_MIN + LIM_DEDOS_MAX - pos) : pos);
}

// ==========================================
// MÁQUINA DE ESTADOS FINITOS (FSM)
// ==========================================
enum EstadoSistema {
    ESTADO_ESPERANDO_SYNC,
    ESTADO_OPERACION_NORMAL,
    ESTADO_FAILSAFE_SIN_SENAL
};

// ==========================================
// OBJETOS Y VARIABLES DE CONTROL
// ==========================================
Servo servoDedos[TOTAL_DEDOS];
Servo servoMuniecaVert;
Servo servoMuniecaRot;

// Cola FreeRTOS para desacoplamiento seguro de datos de radio
QueueHandle_t colaMensajesBrazo = NULL;

// Posiciones angulares actuales y objetivos (con punto flotante para integración suave)
float posActualDedos[TOTAL_DEDOS];
float posObjetivoDedos[TOTAL_DEDOS];

float posActualMuniecaVert  = HOME_MUNIECA_VERT;
float posObjetivoMuniecaVert = HOME_MUNIECA_VERT;

float posActualMuniecaRot   = HOME_MUNIECA_ROT;
float posObjetivoMuniecaRot  = HOME_MUNIECA_ROT;

EstadoSistema estadoActual = ESTADO_ESPERANDO_SYNC;
uint32_t marcaTiempoUltimoPaquete = 0;
bool servosAcoplados = false;

// Prototipos de funciones
void acoplarServos();
void desacoplarServos();
void tareaControlActuadores(void* pvParameters);

// ==========================================
// CALLBACK DE ESP-NOW (Rx) - EJECUTADO EN CORE 0
// ==========================================
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
void OnDataRecv(const esp_now_recv_info_t *esp_now_info, const uint8_t *incomingData, int len) {
#else
void OnDataRecv(const uint8_t* mac, const uint8_t* incomingData, int len) {
#endif
    if (len != sizeof(MensajeBrazo)) {
        return; // Descartar tramas con tamaño incorrecto
    }

    const MensajeBrazo* paquete = reinterpret_cast<const MensajeBrazo*>(incomingData);

    // Validación formal de cabecera mágica y suma de verificación
    if (!validarPaqueteBrazo(*paquete)) {
        return; // Trama corrompida por RF descartada
    }

    // Transferencia thread-safe a la cola de FreeRTOS sin esperas bloqueantes
    if (colaMensajesBrazo != NULL) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        // Sobreescribir el valor en cola si el consumidor aún no lo procesó
        xQueueOverwriteFromISR(colaMensajesBrazo, paquete, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

// ==========================================
// GESTIÓN DE ATTACH / DETACH DE SERVOS
// ==========================================
void acoplarServos() {
    if (servosAcoplados) return;

    for (int i = 0; i < TOTAL_DEDOS; i++) {
        servoDedos[i].setPeriodHertz(50);
        servoDedos[i].attach(PINES_SERVOS_DEDOS[i], 500, 2400);
        servoDedos[i].write(anguloServoDedo(i, posActualDedos[i]));
    }

    servoMuniecaVert.setPeriodHertz(50);
    servoMuniecaVert.attach(PIN_SERVO_MUNIECA_VERT, 500, 2400);
    servoMuniecaVert.write((int)round(posActualMuniecaVert));

    servoMuniecaRot.setPeriodHertz(50);
    servoMuniecaRot.attach(PIN_SERVO_MUNIECA_ROT, 500, 2400);
    servoMuniecaRot.write((int)round(posActualMuniecaRot));

    servosAcoplados = true;
    // Serial.println("[SERVOS] PWM habilitado y actuadores acoplados.");
}

void desacoplarServos() {
    if (!servosAcoplados) return;

    for (int i = 0; i < TOTAL_DEDOS; i++) {
        servoDedos[i].detach();
    }
    servoMuniecaVert.detach();
    servoMuniecaRot.detach();

    servosAcoplados = false;
    // Serial.println("[SERVOS] Actuadores desacoplados (Sleep Mode / Ahorro Térmico).");
}

// ==========================================
// SETUP
// ==========================================
void setup() {
    Serial.begin(115200);
    delay(500);
    // Mensajes informativos comentados para no interferir con el Serial Plotter

    // ── PIN DE SYNC PARA MEDICIÓN DE LATENCIA ──
    pinMode(PIN_SYNC_LATENCIA, INPUT_PULLDOWN);
    attachInterrupt(digitalPinToInterrupt(PIN_SYNC_LATENCIA), isrSyncLatencia, RISING);

    // Inicializar posiciones en Home seguro
    for (int i = 0; i < TOTAL_DEDOS; i++) {
        posActualDedos[i]   = HOME_DEDOS;
        posObjetivoDedos[i] = HOME_DEDOS;
    }
    posActualMuniecaVert   = HOME_MUNIECA_VERT;
    posObjetivoMuniecaVert = HOME_MUNIECA_VERT;
    posActualMuniecaRot    = HOME_MUNIECA_ROT;
    posObjetivoMuniecaRot  = HOME_MUNIECA_ROT;

    // Asignación de temporizadores de hardware LEDC para ESP32Servo
    ESP32PWM::allocateTimer(0);
    ESP32PWM::allocateTimer(1);
    ESP32PWM::allocateTimer(2);
    ESP32PWM::allocateTimer(3);

    // Acoplar servos en posición neutra
    acoplarServos();

    // Crear cola de mensajes (longitud 1 con capacidad de sobreescritura)
    colaMensajesBrazo = xQueueCreate(1, sizeof(MensajeBrazo));
    if (colaMensajesBrazo == NULL) {
        Serial.println("[ERROR FATAL] No se pudo crear la cola FreeRTOS.");
        while (1) delay(1000);
    }

    // Inicializar Radio Wi-Fi y ESP-NOW
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false); // Sin ahorro de energía del modem: evita latencia/pérdida en la recepción

    if (esp_now_init() != ESP_OK) {
        // Fallo en la inicialización de ESP-NOW
        return;
    }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    esp_now_register_recv_cb(OnDataRecv);
#else
    esp_now_register_recv_cb((esp_now_recv_cb_t)OnDataRecv);
#endif

    // Crear Tarea de Control y Actuación en Núcleo 1 (independiente de RF en Core 0)
    xTaskCreatePinnedToCore(
        tareaControlActuadores,
        "TaskServoControl",
        4096,
        NULL,
        3, // Alta prioridad de control en Core 1
        NULL,
        1
    );
}

// ==========================================
// TAREA DE TIEMPO REAL: CONTROL Y SLEW-RATE
// ==========================================
void tareaControlActuadores(void* pvParameters) {
    MensajeBrazo paqueteEntrante;
    uint32_t ticksTelemetria = 0;
    uint32_t tiempoEnHomeMs = 0;

    for (;;) {
        // ─── Variables locales de latencia para este ciclo ───
        bool medirLatenciaEsteCiclo = false;
        uint32_t tsSync = 0;

        // 1. ESPERAR UN PAQUETE: la tarea despierta apenas llega (sin esperar al próximo tick
        // de 50 Hz). Sin paquetes, el timeout mantiene el ciclo a 50 Hz para failsafe y trayectoria.
        if (xQueueReceive(colaMensajesBrazo, &paqueteEntrante, pdMS_TO_TICKS(PERIODO_CONTROL_MS)) == pdTRUE) {
            marcaTiempoUltimoPaquete = millis();

            // Capturar el timestamp del pulso de sync (si hay uno pendiente)
            // Se usa noInterrupts/interrupts para leer atómicamente las variables de la ISR
            noInterrupts();
            if (syncPulsoRecibido) {
                tsSync = timestampSyncMicros;
                syncPulsoRecibido = false;
                medirLatenciaEsteCiclo = true;
            }
            interrupts();

            // Si estábamos desconectados o esperando sync, pasar a operación normal
            if (estadoActual != ESTADO_OPERACION_NORMAL) {
                estadoActual = ESTADO_OPERACION_NORMAL;
                acoplarServos();
                // Serial.printf("[FSM] Enlace establecido. Paquete #%u recibido.\n", paqueteEntrante.secuencia);
            }

            // Actualizar referencias objetivo
            for (int i = 0; i < TOTAL_DEDOS; i++) {
                posObjetivoDedos[i] = constrain((float)paqueteEntrante.anguloDedos[i], LIM_DEDOS_MIN, LIM_DEDOS_MAX);
            }
            posObjetivoMuniecaVert = constrain((float)paqueteEntrante.muniecaVertical, LIM_MUNIECA_VERT_MIN, LIM_MUNIECA_VERT_MAX);
            posObjetivoMuniecaRot  = constrain((float)paqueteEntrante.muniecaRotacional, LIM_MUNIECA_ROT_MIN, LIM_MUNIECA_ROT_MAX);
        }

        // 2. DETECCIÓN DE TIMEOUT Y SUPERVISIÓN DE FAILSAFE
        if (estadoActual == ESTADO_OPERACION_NORMAL) {
            if (millis() - marcaTiempoUltimoPaquete > TIMEOUT_FAILSAFE_MS) {
                estadoActual = ESTADO_FAILSAFE_SIN_SENAL;
                // Serial.println("[ALERTA FAILSAFE] Pérdida de señal RF detectada. Comandando pose Home segura...");
                // Fijar objetivos a la posición Home
                for (int i = 0; i < TOTAL_DEDOS; i++) {
                    posObjetivoDedos[i] = HOME_DEDOS;
                }
                posObjetivoMuniecaVert = HOME_MUNIECA_VERT;
                posObjetivoMuniecaRot  = HOME_MUNIECA_ROT;
                tiempoEnHomeMs = millis();
            }
        }

        // 3. GENERADOR DE TRAYECTORIA: SLEW-RATE LIMITING + DEADBAND
        // Control cinemático de los 5 dedos (SG90)
        bool todosEnReposo = true;

        for (int i = 0; i < TOTAL_DEDOS; i++) {
            float error = posObjetivoDedos[i] - posActualDedos[i];

            if (fabs(error) > DEADBAND_DEDOS) {
                todosEnReposo = false;
                float incremento = constrain(error, -VEL_MAX_DEDOS, VEL_MAX_DEDOS);
                posActualDedos[i] += incremento;
                if (servosAcoplados) {
                    servoDedos[i].write(anguloServoDedo(i, posActualDedos[i]));
                }
            }
        }

        // Control cinemático Muñeca Vertical (MG946R)
        float errorVert = posObjetivoMuniecaVert - posActualMuniecaVert;
        if (fabs(errorVert) > DEADBAND_MUNIECA) {
            todosEnReposo = false;
            float incrementoVert = constrain(errorVert, -VEL_MAX_MUNIECA_VERT, VEL_MAX_MUNIECA_VERT);
            posActualMuniecaVert += incrementoVert;
            if (servosAcoplados) {
                servoMuniecaVert.write((int)round(posActualMuniecaVert));
            }
        }

        // Control cinemático Muñeca Rotacional (MG946R)
        float errorRot = posObjetivoMuniecaRot - posActualMuniecaRot;
        if (fabs(errorRot) > DEADBAND_MUNIECA) {
            todosEnReposo = false;
            float incrementoRot = constrain(errorRot, -VEL_MAX_MUNIECA_ROT, VEL_MAX_MUNIECA_ROT);
            posActualMuniecaRot += incrementoRot;
            if (servosAcoplados) {
                servoMuniecaRot.write((int)round(posActualMuniecaRot));
            }
        }

        // ─────────────────────────────────────────────────────────
        // MEDICIÓN DE LATENCIA: salida para Arduino Serial Plotter
        // Se calcula inmediatamente después de escribir todos los actuadores
        // ─────────────────────────────────────────────────────────
        if (medirLatenciaEsteCiclo) {
            uint32_t tsFinServos = micros();
            uint32_t latenciaUs = tsFinServos - tsSync;

            // Protección contra desbordamiento de micros() (wraps cada ~71 min)
            if (latenciaUs < 500000) {  // Descartar valores absurdos (>500 ms)
                // Actualizar estadísticas acumuladas
                latenciaContador++;
                latenciaSumaUs += latenciaUs;
                if (latenciaUs < latenciaMinUs) latenciaMinUs = latenciaUs;
                if (latenciaUs > latenciaMaxUs) latenciaMaxUs = latenciaUs;

                // Salida directa para Arduino Serial Plotter (en ms)
                float latenciaMs = (float)latenciaUs / 1000.0f;
                Serial.printf("Latencia_ms:%.2f\n", latenciaMs);
            }
        }

        // 4. GESTIÓN DE DESACOPLE EN FAILSAFE SOSTENIDO
        // Si el sistema está en Failsafe y ya alcanzó la posición Home, tras 2 segundos se desacoplan los servos
        if (estadoActual == ESTADO_FAILSAFE_SIN_SENAL && todosEnReposo && servosAcoplados) {
            if (millis() - tiempoEnHomeMs > 2000) {
                desacoplarServos();
            }
        }

        // 5. Telemetría de texto deshabilitada para no ensuciar el Serial Plotter
    }
}

// ==========================================
// LOOP (VACÍO - TAREAS ASIGNADAS A FREERTOS)
// ==========================================
void loop() {
    // Comando 'r' para resetear estadísticas de latencia (sin texto para no corromper el plotter)
    if (Serial.available() > 0) {
        char cmd = Serial.read();
        if (cmd == 'r' || cmd == 'R') {
            latenciaMinUs    = UINT32_MAX;
            latenciaMaxUs    = 0;
            latenciaSumaUs   = 0;
            latenciaContador = 0;
        }
    }
    vTaskDelay(pdMS_TO_TICKS(500));
}
