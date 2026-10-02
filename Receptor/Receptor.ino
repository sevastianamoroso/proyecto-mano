/**
 * @file Receptor.ino
 * @brief Firmware de Recepción y Control Cinemático Robótico con FreeRTOS y ESP32
 * @author Revisión de Grado en Ingeniería Electrónica y Sistemas Embebidos
 * 
 * Mejoras clave implementadas:
 *  - Arquitectura orientada a eventos con Colas FreeRTOS (desacoplo total de ISR WiFi y Actuadores)
 *  - Eliminación de Race Conditions (cero variables globales modificadas desde callback)
 *  - Generador de trayectoria con Limitador de Velocidad (Slew-Rate Limiter) para proteger engranajes y piezas 3D
 *  - Banda muerta (Deadband) en todos los servos para eliminar el zumbido (jitter/hunting) y consumo estático
 *  - Máquina de Estados con Failsafe activo ante pérdida de enlace RF o reinicio del transmisor
 *  - Retorno suave a pose Home neutra y desacople de torque (detach) en reposo prolongado
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_idf_version.h>
#include <ESP32Servo.h>

#include "ProtocoloBrazo.h"

// ==========================================
// CONFIGURACIÓN DE PINES DE SERVOMOTORES
// ==========================================
const int PIN_SERVO_PULGAR        = 25;
const int PIN_SERVO_INDICE        = 26;
const int PIN_SERVO_MEDIO         = 27;
const int PIN_SERVO_ANULAR        = 12;
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
// SG90 (dedos): 3.0° / 20ms = 150°/segundo
const float VEL_MAX_DEDOS = 3.0f;
// MG946R (muñeca vertical - mayor inercia y carga): 1.5° / 20ms = 75°/segundo
const float VEL_MAX_MUNIECA_VERT = 1.5f;
// MG946R (muñeca rotacional): 2.0° / 20ms = 100°/segundo
const float VEL_MAX_MUNIECA_ROT = 2.0f;

// Posiciones neutras de reposo seguro (Home)
const float HOME_DEDOS        = 25.0f; // Mano abierta relajada
const float HOME_MUNIECA_VERT = 57.0f; // Posición media horizontal
const float HOME_MUNIECA_ROT  = 90.0f; // Muñeca en ángulo neutro

// Límites mecánicos absolutos (protección de la mano 3D ante paquetes fuera de rango)
const float LIM_DEDOS_MIN = 25.0f,        LIM_DEDOS_MAX = 90.0f;
const float LIM_MUNIECA_VERT_MIN = 25.0f, LIM_MUNIECA_VERT_MAX = 90.0f;
const float LIM_MUNIECA_ROT_MIN = 0.0f,   LIM_MUNIECA_ROT_MAX = 180.0f;

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
        servoDedos[i].write((int)round(posActualDedos[i]));
    }

    servoMuniecaVert.setPeriodHertz(50);
    servoMuniecaVert.attach(PIN_SERVO_MUNIECA_VERT, 500, 2400);
    servoMuniecaVert.write((int)round(posActualMuniecaVert));

    servoMuniecaRot.setPeriodHertz(50);
    servoMuniecaRot.attach(PIN_SERVO_MUNIECA_ROT, 500, 2400);
    servoMuniecaRot.write((int)round(posActualMuniecaRot));

    servosAcoplados = true;
    Serial.println("[SERVOS] PWM habilitado y actuadores acoplados.");
}

void desacoplarServos() {
    if (!servosAcoplados) return;

    for (int i = 0; i < TOTAL_DEDOS; i++) {
        servoDedos[i].detach();
    }
    servoMuniecaVert.detach();
    servoMuniecaRot.detach();

    servosAcoplados = false;
    Serial.println("[SERVOS] Actuadores desacoplados (Sleep Mode / Ahorro Térmico).");
}

// ==========================================
// SETUP
// ==========================================
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n[SISTEMA] Iniciando Receptor y Controlador Cinemático...");

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
    Serial.print("[INFO] Dirección MAC Receptora: ");
    Serial.println(WiFi.macAddress());

    if (esp_now_init() != ESP_OK) {
        Serial.println("[ERROR FATAL] Fallo en la inicialización de ESP-NOW.");
        return;
    }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    esp_now_register_recv_cb(OnDataRecv);
#else
    esp_now_register_recv_cb((esp_now_recv_cb_t)OnDataRecv);
#endif
    Serial.println("[ESP-NOW] Receptor a la escucha de paquetes...");

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
    TickType_t xLastWakeTime = xTaskGetTickCount();
    MensajeBrazo paqueteEntrante;
    uint32_t ticksTelemetria = 0;
    uint32_t tiempoEnHomeMs = 0;

    for (;;) {
        // Ejecución estrictamente periódica a 50 Hz
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(PERIODO_CONTROL_MS));

        // 1. REVISAR SI LLEGARON NUEVOS DATOS DESDE LA COLA
        if (xQueueReceive(colaMensajesBrazo, &paqueteEntrante, 0) == pdTRUE) {
            marcaTiempoUltimoPaquete = millis();

            // Si estábamos desconectados o esperando sync, pasar a operación normal
            if (estadoActual != ESTADO_OPERACION_NORMAL) {
                estadoActual = ESTADO_OPERACION_NORMAL;
                acoplarServos();
                Serial.printf("[FSM] Enlace establecido. Paquete #%u recibido.\n", paqueteEntrante.secuencia);
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
                Serial.println("[ALERTA FAILSAFE] Pérdida de señal RF detectada. Comandando pose Home segura...");
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
                    servoDedos[i].write((int)round(posActualDedos[i]));
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

        // 4. GESTIÓN DE DESACOPLE EN FAILSAFE SOSTENIDO
        // Si el sistema está en Failsafe y ya alcanzó la posición Home, tras 2 segundos se desacoplan los servos
        if (estadoActual == ESTADO_FAILSAFE_SIN_SENAL && todosEnReposo && servosAcoplados) {
            if (millis() - tiempoEnHomeMs > 2000) {
                desacoplarServos();
            }
        }

        // 5. TELEMETRÍA SERIAL NO BLOQUEANTE (5 Hz)
        if (++ticksTelemetria >= 10) {
            ticksTelemetria = 0;
            const char* strEstado = (estadoActual == ESTADO_OPERACION_NORMAL) ? "OK" :
                                    (estadoActual == ESTADO_FAILSAFE_SIN_SENAL) ? "FAILSAFE" : "SYNC_WAIT";
            Serial.printf("[RX %s] D1:%.1f D2:%.1f D3:%.1f D4:%.1f D5:%.1f | Mñc:[V:%.1f, R:%.1f]\n",
                          strEstado,
                          posActualDedos[0], posActualDedos[1], posActualDedos[2],
                          posActualDedos[3], posActualDedos[4],
                          posActualMuniecaVert, posActualMuniecaRot);
        }
    }
}

// ==========================================
// LOOP (VACÍO - TAREAS ASIGNADAS A FREERTOS)
// ==========================================
void loop() {
    vTaskDelay(pdMS_TO_TICKS(500));
}
