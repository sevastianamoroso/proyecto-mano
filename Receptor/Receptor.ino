// Receptor.ino - firmware de la mano.
//
// Recibe por ESP-NOW los ángulos que manda el guante y mueve los 7 servos
// (5 dedos y 2 de la muñeca). No hace ninguna cuenta con los sensores: sólo
// revisa que el paquete llegue sano, limita la velocidad para no castigar la
// mecánica y, si se corta el enlace, lleva la mano a una posición de reposo.

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_idf_version.h>
#include <ESP32Servo.h>

#include "ProtocoloBrazo.h"

// ---------- Pines de los servos ----------

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

// ---------- Parámetros ----------

// Período de la tarea de control: 20 ms = 50 Hz, igual que el guante
const TickType_t PERIODO_CONTROL_MS = 20;

// Si pasan 250 ms sin paquetes (unos 12 perdidos seguidos) damos el enlace por caído
const uint32_t TIMEOUT_FAILSAFE_MS = 250;

// Diferencias menores a esto (°) no se corrigen; si no, el servo vive temblando
// entre dos posiciones y zumbando
const float DEADBAND_DEDOS   = 1.0f;
const float DEADBAND_MUNIECA = 1.2f;

// Cuánto puede moverse cada servo en un ciclo de 20 ms (°). Es el limitador de
// velocidad: evita golpes en los engranajes y en las piezas impresas.
// Dedos (SG90): 8° por ciclo = 400°/s; el SG90 sin carga llega a ~600°/s
const float VEL_MAX_DEDOS = 8.0f;
// Muñeca vertical (MG946R): carga con todo el peso de la mano, así que más despacio, 200°/s
const float VEL_MAX_MUNIECA_VERT = 4.0f;
// Muñeca rotacional (MG946R): 250°/s
const float VEL_MAX_MUNIECA_ROT = 5.0f;

// Según cómo quedó montado cada servo (pulgar..meñique), algunos cierran el dedo
// bajando el ángulo en vez de subiéndolo. En esos va true y se espeja dentro de 25..90°.
// Se ajusta por dedo en la prueba C3: con el guante abierto, el dedo de la mano tiene que quedar abierto.
const bool INVERTIR_DEDO[TOTAL_DEDOS] = {true, true, true, true, true};

// Posición de reposo (a donde va la mano al arrancar o si se corta el enlace)
const float HOME_DEDOS        = 25.0f; // mano abierta
const float HOME_MUNIECA_VERT = 57.0f; // muñeca derecha
const float HOME_MUNIECA_ROT  = 90.0f; // sin girar

// Topes de cada articulación. El guante ya manda valores dentro de estos rangos,
// pero los volvemos a recortar acá por si llega cualquier cosa.
const float LIM_DEDOS_MIN = 25.0f,        LIM_DEDOS_MAX = 90.0f;
const float LIM_MUNIECA_VERT_MIN = 25.0f, LIM_MUNIECA_VERT_MAX = 90.0f;
const float LIM_MUNIECA_ROT_MIN = 0.0f,   LIM_MUNIECA_ROT_MAX = 180.0f;

// Pasa del ángulo del dedo (25 = abierto, 90 = cerrado) al ángulo que hay que
// mandarle al servo, teniendo en cuenta si está invertido
static inline int anguloServoDedo(int i, float pos) {
    return (int)round(INVERTIR_DEDO[i] ? (LIM_DEDOS_MIN + LIM_DEDOS_MAX - pos) : pos);
}

// ---------- Estados ----------

enum EstadoSistema {
    ESTADO_ESPERANDO_SYNC,     // recién arrancado, todavía no llegó ningún paquete
    ESTADO_OPERACION_NORMAL,   // llegan paquetes y la mano sigue al guante
    ESTADO_FAILSAFE_SIN_SENAL  // se cortó el enlace: la mano vuelve al reposo
};

// ---------- Variables globales ----------

Servo servoDedos[TOTAL_DEDOS];
Servo servoMuniecaVert;
Servo servoMuniecaRot;

// Por acá pasan los paquetes del callback a la tarea de control
QueueHandle_t colaMensajesBrazo = NULL;

// Para cada servo: dónde está ahora y a dónde tiene que llegar. Son float para
// que el avance de a poco del limitador no se pierda redondeando.
float posActualDedos[TOTAL_DEDOS];
float posObjetivoDedos[TOTAL_DEDOS];

float posActualMuniecaVert  = HOME_MUNIECA_VERT;
float posObjetivoMuniecaVert = HOME_MUNIECA_VERT;

float posActualMuniecaRot   = HOME_MUNIECA_ROT;
float posObjetivoMuniecaRot  = HOME_MUNIECA_ROT;

EstadoSistema estadoActual = ESTADO_ESPERANDO_SYNC;
uint32_t marcaTiempoUltimoPaquete = 0;
bool servosAcoplados = false;

void acoplarServos();
void desacoplarServos();
void tareaControlActuadores(void* pvParameters);

// ---------- Callback de ESP-NOW ----------

// Lo llama la pila WiFi (en el núcleo 0) cada vez que llega un paquete. Tiene que
// ser corto: revisa el paquete y lo deja en la cola, nada más. La firma cambió
// en el ESP-IDF 5, por eso el #if.
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
void OnDataRecv(const esp_now_recv_info_t *esp_now_info, const uint8_t *incomingData, int len) {
#else
void OnDataRecv(const uint8_t* mac, const uint8_t* incomingData, int len) {
#endif
    if (len != sizeof(MensajeBrazo)) {
        return; // no es un paquete nuestro
    }

    const MensajeBrazo* paquete = reinterpret_cast<const MensajeBrazo*>(incomingData);

    // Cabecera y checksum: si algo no cierra, el paquete se tira
    if (!validarPaqueteBrazo(*paquete)) {
        return;
    }

    // La cola tiene lugar para un solo paquete y se sobrescribe: si la tarea todavía
    // no leyó el anterior, se pierde el viejo y queda el más nuevo, que es el que importa
    if (colaMensajesBrazo != NULL) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xQueueOverwriteFromISR(colaMensajesBrazo, paquete, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

// ---------- Prender y apagar los servos ----------

// attach() arranca el PWM. Antes de eso cada servo recibe la posición actual,
// para que no pegue un salto al engancharse.
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
    Serial.println("[SERVOS] PWM habilitado y actuadores acoplados.");
}

// Sin PWM los servos dejan de hacer fuerza: no consumen ni calientan mientras
// la mano está en reposo esperando al guante
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

// ---------- setup ----------

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n[SISTEMA] Iniciando Receptor y Controlador Cinemático...");

    // Arrancamos con todo en la posición de reposo
    for (int i = 0; i < TOTAL_DEDOS; i++) {
        posActualDedos[i]   = HOME_DEDOS;
        posObjetivoDedos[i] = HOME_DEDOS;
    }
    posActualMuniecaVert   = HOME_MUNIECA_VERT;
    posObjetivoMuniecaVert = HOME_MUNIECA_VERT;
    posActualMuniecaRot    = HOME_MUNIECA_ROT;
    posObjetivoMuniecaRot  = HOME_MUNIECA_ROT;

    // ESP32Servo genera el PWM con los timers del periférico LEDC; le reservamos los 4
    ESP32PWM::allocateTimer(0);
    ESP32PWM::allocateTimer(1);
    ESP32PWM::allocateTimer(2);
    ESP32PWM::allocateTimer(3);

    acoplarServos();

    // Cola de un solo lugar (ver el callback)
    colaMensajesBrazo = xQueueCreate(1, sizeof(MensajeBrazo));
    if (colaMensajesBrazo == NULL) {
        Serial.println("[ERROR FATAL] No se pudo crear la cola FreeRTOS.");
        while (1) delay(1000);
    }

    // WiFi y ESP-NOW
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false); // con el ahorro de energía la radio puede tardar o perder paquetes
    // La MAC que se imprime acá es la que hay que poner en macReceptor del transmisor
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

    // La tarea de control va en el núcleo 1, separada del WiFi que corre en el 0
    xTaskCreatePinnedToCore(
        tareaControlActuadores,
        "TaskServoControl",
        4096,
        NULL,
        3,    // por encima de loop()
        NULL,
        1
    );
}

// ---------- Tarea de control ----------

void tareaControlActuadores(void* pvParameters) {
    MensajeBrazo paqueteEntrante;
    uint32_t ticksTelemetria = 0;
    uint32_t tiempoEnHomeMs = 0;

    for (;;) {
        // --- 1. Esperar un paquete ---
        // La tarea se despierta apenas llega uno, sin esperar al próximo ciclo. Si no
        // llega nada en 20 ms sale igual, así el failsafe y el movimiento siguen a 50 Hz.
        // Ojo: el limitador mueve una cantidad fija por vuelta y eso da bien porque el
        // guante manda a 50 Hz. Si se sube la tasa del guante, hay que escalar los
        // VEL_MAX_* con el tiempo real entre vueltas.
        if (xQueueReceive(colaMensajesBrazo, &paqueteEntrante, pdMS_TO_TICKS(PERIODO_CONTROL_MS)) == pdTRUE) {
            marcaTiempoUltimoPaquete = millis();

            // Primer paquete, o volvió el enlace después de un corte
            if (estadoActual != ESTADO_OPERACION_NORMAL) {
                estadoActual = ESTADO_OPERACION_NORMAL;
                acoplarServos();
                Serial.printf("[FSM] Enlace establecido. Paquete #%u recibido.\n", paqueteEntrante.secuencia);
            }

            // Nuevos objetivos, recortados a los topes
            for (int i = 0; i < TOTAL_DEDOS; i++) {
                posObjetivoDedos[i] = constrain((float)paqueteEntrante.anguloDedos[i], LIM_DEDOS_MIN, LIM_DEDOS_MAX);
            }
            posObjetivoMuniecaVert = constrain((float)paqueteEntrante.muniecaVertical, LIM_MUNIECA_VERT_MIN, LIM_MUNIECA_VERT_MAX);
            posObjetivoMuniecaRot  = constrain((float)paqueteEntrante.muniecaRotacional, LIM_MUNIECA_ROT_MIN, LIM_MUNIECA_ROT_MAX);
        }

        // --- 2. ¿Se cortó el enlace? ---
        if (estadoActual == ESTADO_OPERACION_NORMAL) {
            if (millis() - marcaTiempoUltimoPaquete > TIMEOUT_FAILSAFE_MS) {
                estadoActual = ESTADO_FAILSAFE_SIN_SENAL;
                Serial.println("[ALERTA FAILSAFE] Pérdida de señal RF detectada. Comandando pose Home segura...");
                // Mandamos todo al reposo; el limitador se encarga de que vaya despacio
                for (int i = 0; i < TOTAL_DEDOS; i++) {
                    posObjetivoDedos[i] = HOME_DEDOS;
                }
                posObjetivoMuniecaVert = HOME_MUNIECA_VERT;
                posObjetivoMuniecaRot  = HOME_MUNIECA_ROT;
                tiempoEnHomeMs = millis();
            }
        }

        // --- 3. Mover cada servo un paso hacia su objetivo ---
        // Si la diferencia es menor que la banda muerta no se toca; si es mayor, se
        // avanza como mucho VEL_MAX_* por vuelta
        bool todosEnReposo = true;

        // Dedos (SG90)
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

        // Muñeca vertical (MG946R)
        float errorVert = posObjetivoMuniecaVert - posActualMuniecaVert;
        if (fabs(errorVert) > DEADBAND_MUNIECA) {
            todosEnReposo = false;
            float incrementoVert = constrain(errorVert, -VEL_MAX_MUNIECA_VERT, VEL_MAX_MUNIECA_VERT);
            posActualMuniecaVert += incrementoVert;
            if (servosAcoplados) {
                servoMuniecaVert.write((int)round(posActualMuniecaVert));
            }
        }

        // Muñeca rotacional (MG946R)
        float errorRot = posObjetivoMuniecaRot - posActualMuniecaRot;
        if (fabs(errorRot) > DEADBAND_MUNIECA) {
            todosEnReposo = false;
            float incrementoRot = constrain(errorRot, -VEL_MAX_MUNIECA_ROT, VEL_MAX_MUNIECA_ROT);
            posActualMuniecaRot += incrementoRot;
            if (servosAcoplados) {
                servoMuniecaRot.write((int)round(posActualMuniecaRot));
            }
        }

        // --- 4. Apagar los servos si el corte sigue ---
        // En failsafe, una vez que la mano llegó al reposo y pasaron 2 s, se apagan
        if (estadoActual == ESTADO_FAILSAFE_SIN_SENAL && todosEnReposo && servosAcoplados) {
            if (millis() - tiempoEnHomeMs > 2000) {
                desacoplarServos();
            }
        }

        // --- 5. Telemetría por serie, 1 de cada 10 vueltas (5 Hz) ---
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

// ---------- loop ----------

// No hace nada: todo pasa en el callback y en la tarea de control
void loop() {
    vTaskDelay(pdMS_TO_TICKS(500));
}
