// Receptor_Latencia.ino - el receptor normal + la medición de latencia de punta a punta
// (versión 2 del método).
//
// Es una copia de Receptor/Receptor.ino. Todo lo agregado está marcado con [LATENCIA];
// el resto (recepción, cola, FSM, slew-rate, banda muerta, failsafe) no se tocó.
// El transmisor usa su firmware normal (Transmisor/Transmisor.ino), sin cambios.
//
// Idea: el receptor genera un escalón eléctrico que reemplaza al potenciómetro del índice
// en el guante y mide cuánto tarda en cambiar el pulso PWM que recibe el servo del índice.
// El inicio y el fin se toman con el reloj de esta placa, así que no hay que sincronizar nada.
//
// Conexiones:
//    Rx GPIO 18 ──[1 kΩ]──  Tx GPIO 34   (cursor del potenciómetro del índice DESCONECTADO)
//    Rx GPIO 26 ──────────  Rx GPIO 4    (puente: la señal del servo del índice vuelve a entrar)
//    Rx GND     ──────────  Tx GND
//
// Por cada escalón imprime una línea:
//    L,n,nivel,secuencia,llegada_us,primer_us,final_us,pulsos_perdidos
//      llegada_us: escalón -> primer paquete que lo trae, validado en el callback
//      primer_us:  escalón -> fin del primer pulso PWM con el ancho cambiado (punta a punta)
//      final_us:   escalón -> fin del primer pulso con el ancho final (incluye el slew-rate)
//    T,n,nivel,etapa   si el ensayo no terminó en 1 s (etapa 0: no llegó el paquete,
//                      1: llegó pero el PWM no cambió, 2: el PWM no se estabilizó)
// Comandos por el monitor serie: 's' resumen, 'r' borrar estadísticas, 'p' pausar/reanudar,
// 'g' alternar la salida para el Serial Plotter (latencia de cada ensayo y media acumulada).

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_idf_version.h>
#include <ESP32Servo.h>

#include "ProtocoloBrazo.h"

// ---------- [LATENCIA] Configuración de la prueba ----------

const int DEDO_PRUEBA     = DEDO_INDICE; // su potenciómetro se reemplaza por el escalón
const int PIN_ESTIMULO    = 18;          // salida hacia el GPIO 34 del Tx
const int PIN_CAPTURA_PWM = 4;           // entrada, puenteada con el GPIO 26 (servo del índice)

const float    UMBRAL_PAQUETE_DEG = 3.0f;  // cambio en el paquete que cuenta como llegada (> banda muerta)
const uint32_t UMBRAL_CAMBIO_US   = 30;    // cambio de ancho que cuenta como orden nueva (1° ≈ 10,6 µs)
const uint32_t UMBRAL_QUIETO_US   = 8;     // diferencia máxima entre pulsos para decir que no cambió
const int      PULSOS_QUIETO      = 5;     // pulsos iguales seguidos para dar la orden por establecida
const uint32_t TIMEOUT_ENSAYO_MS  = 1000;
const uint32_t ESPERA_MIN_MS      = 200;   // espera aleatoria entre ensayos, para que el escalón caiga
const uint32_t ESPERA_MAX_MS      = 600;   // en cualquier punto del ciclo de 20 ms del Tx
const int      MAX_MUESTRAS       = 2000;  // para el resumen por el monitor serie

portMUX_TYPE muxLatencia = portMUX_INITIALIZER_UNLOCKED;

// Escritas por la ISR de captura PWM
volatile uint32_t tUltimoFlancoUs = 0;
volatile uint32_t anchoPulsoUs    = 0;  // ancho del último pulso completo
volatile uint32_t finPulsoUs      = 0;  // instante de su flanco de bajada
volatile uint32_t contadorPulsos  = 0;

// Escritas por el callback de ESP-NOW
volatile uint16_t ultimoValorDedo  = 0;
volatile bool     ensayoArmado     = false;
volatile uint16_t valorDedoAntes   = 0;
volatile bool     llegoEscalon     = false;
volatile uint32_t llegadaUs        = 0;
volatile uint8_t  secuenciaLlegada = 0;

// 'g': salida para el Serial Plotter (solo latencia y media, sin telemetría ni CSV)
volatile bool modoPlotter = false;

// El pulso alto dura 0,5–2,4 ms y el bajo más de 17 ms: un intervalo corto entre flancos
// es un pulso que acaba de terminar. Así no hace falta leer el nivel del pin.
void IRAM_ATTR isrCapturaPwm() {
    uint32_t ahora = micros();
    uint32_t dt = ahora - tUltimoFlancoUs;
    tUltimoFlancoUs = ahora;
    if (dt < 3000) {
        portENTER_CRITICAL_ISR(&muxLatencia);
        anchoPulsoUs = dt;
        finPulsoUs = ahora;
        contadorPulsos++;
        portEXIT_CRITICAL_ISR(&muxLatencia);
    }
}

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

    // [LATENCIA] Primer paquete que ya trae el escalón del dedo de prueba
    uint32_t ahoraUs = micros();
    uint16_t valor = paquete->anguloDedos[DEDO_PRUEBA];
    portENTER_CRITICAL(&muxLatencia);
    ultimoValorDedo = valor;
    if (ensayoArmado && !llegoEscalon &&
        abs((int)valor - (int)valorDedoAntes) >= UMBRAL_PAQUETE_DEG) {
        llegadaUs = ahoraUs;
        secuenciaLlegada = paquete->secuencia;
        llegoEscalon = true;
    }
    portEXIT_CRITICAL(&muxLatencia);

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

    // [LATENCIA] Escalón hacia el Tx y captura del PWM del servo del índice
    pinMode(PIN_ESTIMULO, OUTPUT);
    digitalWrite(PIN_ESTIMULO, LOW);
    pinMode(PIN_CAPTURA_PWM, INPUT);
    attachInterrupt(digitalPinToInterrupt(PIN_CAPTURA_PWM), isrCapturaPwm, CHANGE);
    Serial.printf("[LATENCIA] Estímulo en GPIO %d, captura PWM en GPIO %d (puente desde GPIO %d)\n",
                  PIN_ESTIMULO, PIN_CAPTURA_PWM, PINES_SERVOS_DEDOS[DEDO_PRUEBA]);
    Serial.println("# L,n,nivel,secuencia,llegada_us,primer_us,final_us,pulsos_perdidos");

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
        if (++ticksTelemetria >= 10 && !modoPlotter) {
            ticksTelemetria = 0;
            const char* strEstado = (estadoActual == ESTADO_OPERACION_NORMAL) ? "OK" :
                                    (estadoActual == ESTADO_FAILSAFE_SIN_SENAL) ? "FAILSAFE" : "SYNC_WAIT";
            // [LATENCIA] Pulsos vistos en el GPIO 4: si no aumentan (~50/s), el puente desde el GPIO 26 no llega
            Serial.printf("[RX %s] D1:%.1f D2:%.1f D3:%.1f D4:%.1f D5:%.1f | Mñc:[V:%.1f, R:%.1f] | GPIO%d: %u pulsos, %u us\n",
                          strEstado,
                          posActualDedos[0], posActualDedos[1], posActualDedos[2],
                          posActualDedos[3], posActualDedos[4],
                          posActualMuniecaVert, posActualMuniecaRot,
                          PIN_CAPTURA_PWM, contadorPulsos, anchoPulsoUs);
        }
    }
}

// ---------- [LATENCIA] Ensayos de escalón (en loop) ----------

// Corre en loop() (prioridad 1, núcleo 1), por debajo de la tarea de control. Los instantes
// los toman la ISR y el callback, así que una demora de loop() no cambia los resultados.
enum EstadoEnsayo { ENSAYO_ESPERA, ENSAYO_MIDIENDO };

EstadoEnsayo estadoEnsayo = ENSAYO_ESPERA;
bool     pausado = false;
bool     nivelEstimulo = false;
uint32_t numeroEnsayo = 0;
uint32_t esperaHastaMs = 0;
uint32_t t0Us = 0, t0Ms = 0;

// Seguimiento de los pulsos
uint32_t pulsosVistos = 0;
uint32_t anchoAnterior = 0;
int      pulsosIguales = 0;
uint32_t inicioIgualesUs = 0;

// Datos del ensayo en curso
uint32_t anchoBase = 0;
bool     primerVisto = false;
uint32_t primerUs = 0;
uint32_t perdidosEnsayo = 0;

// Resumen
uint32_t muestrasPrimer[MAX_MUESTRAS];
uint32_t cantMuestras = 0, cantTimeouts = 0;
uint64_t sumaPrimer = 0;

int compararU32(const void* a, const void* b) {
    uint32_t x = *(const uint32_t*)a, y = *(const uint32_t*)b;
    return (x > y) - (x < y);
}

void imprimirResumen() {
    if (cantMuestras == 0) {
        Serial.printf("[RESUMEN] Sin ensayos válidos (timeouts: %u)\n", cantTimeouts);
        return;
    }
    static uint32_t orden[MAX_MUESTRAS];
    memcpy(orden, muestrasPrimer, cantMuestras * sizeof(uint32_t));
    qsort(orden, cantMuestras, sizeof(uint32_t), compararU32);
    uint64_t suma = 0;
    for (uint32_t i = 0; i < cantMuestras; i++) suma += orden[i];
    Serial.printf("[RESUMEN] n=%u timeouts=%u | punta a punta (ms): media %.2f, min %.2f, "
                  "mediana %.2f, p95 %.2f, max %.2f\n",
                  cantMuestras, cantTimeouts, suma / 1000.0 / cantMuestras,
                  orden[0] / 1000.0, orden[cantMuestras / 2] / 1000.0,
                  orden[(cantMuestras * 95) / 100] / 1000.0, orden[cantMuestras - 1] / 1000.0);
}

void terminarEnsayo() {
    portENTER_CRITICAL(&muxLatencia);
    ensayoArmado = false;
    portEXIT_CRITICAL(&muxLatencia);
    estadoEnsayo = ENSAYO_ESPERA;
    esperaHastaMs = millis() + ESPERA_MIN_MS + esp_random() % (ESPERA_MAX_MS - ESPERA_MIN_MS);
}

void loop() {
    if (Serial.available() > 0) {
        char cmd = Serial.read();
        if (cmd == 's' || cmd == 'S') imprimirResumen();
        if (cmd == 'r' || cmd == 'R') { cantMuestras = 0; cantTimeouts = 0; sumaPrimer = 0; Serial.println("[RESUMEN] Borrado"); }
        if (cmd == 'p' || cmd == 'P') { pausado = !pausado; Serial.println(pausado ? "[LATENCIA] Pausa" : "[LATENCIA] Sigue"); }
        if (cmd == 'g' || cmd == 'G') modoPlotter = !modoPlotter;
    }

    // Copia atómica de lo que dejaron la ISR y el callback
    portENTER_CRITICAL(&muxLatencia);
    uint32_t cont = contadorPulsos, ancho = anchoPulsoUs, fin = finPulsoUs;
    bool llego = llegoEscalon;
    uint32_t llegada = llegadaUs;
    uint8_t seq = secuenciaLlegada;
    portEXIT_CRITICAL(&muxLatencia);

    // Un pulso nuevo cada 20 ms; loop() mira cada 1 ms. Si se salteó alguno, se anota.
    if (cont != pulsosVistos) {
        if (estadoEnsayo == ENSAYO_MIDIENDO && cont - pulsosVistos > 1) perdidosEnsayo += cont - pulsosVistos - 1;
        pulsosVistos = cont;

        uint32_t dif = (ancho > anchoAnterior) ? ancho - anchoAnterior : anchoAnterior - ancho;
        if (dif <= UMBRAL_QUIETO_US) {
            pulsosIguales++;
        } else {
            pulsosIguales = 1;
            inicioIgualesUs = fin;
        }
        anchoAnterior = ancho;

        if (estadoEnsayo == ENSAYO_MIDIENDO && (int32_t)(fin - t0Us) > 0) {
            uint32_t difBase = (ancho > anchoBase) ? ancho - anchoBase : anchoBase - ancho;
            if (!primerVisto && difBase > UMBRAL_CAMBIO_US) {
                primerVisto = true;
                primerUs = fin - t0Us;
            }
            if (primerVisto && pulsosIguales >= PULSOS_QUIETO && (int32_t)(inicioIgualesUs - t0Us) > 0) {
                uint32_t finalUs = inicioIgualesUs - t0Us;
                if (cantMuestras < MAX_MUESTRAS) {
                    muestrasPrimer[cantMuestras++] = primerUs;
                    sumaPrimer += primerUs;
                }
                if (modoPlotter) {
                    Serial.printf("latencia_ms:%.2f media_ms:%.2f\n", primerUs / 1000.0,
                                  sumaPrimer / 1000.0 / cantMuestras);
                } else {
                    Serial.printf("L,%u,%d,%u,%lu,%lu,%lu,%u\n", numeroEnsayo, nivelEstimulo ? 1 : 0, seq,
                                  llego ? (unsigned long)(llegada - t0Us) : 0UL,
                                  (unsigned long)primerUs, (unsigned long)finalUs, perdidosEnsayo);
                }
                terminarEnsayo();
            }
        }
    }

    if (estadoEnsayo == ENSAYO_MIDIENDO && millis() - t0Ms > TIMEOUT_ENSAYO_MS) {
        if (!modoPlotter) Serial.printf("T,%u,%d,%d\n", numeroEnsayo, nivelEstimulo ? 1 : 0, !llego ? 0 : (!primerVisto ? 1 : 2));
        cantTimeouts++;
        terminarEnsayo();
    }

    // Arrancar un ensayo: enlace activo, servo quieto y espera aleatoria cumplida
    bool pulsosRecientes = (micros() - fin) < 50000;
    if (estadoEnsayo == ENSAYO_ESPERA && !pausado && estadoActual == ESTADO_OPERACION_NORMAL &&
        servosAcoplados && pulsosRecientes && pulsosIguales >= PULSOS_QUIETO &&
        (int32_t)(millis() - esperaHastaMs) >= 0) {
        anchoBase = ancho;
        primerVisto = false;
        perdidosEnsayo = 0;
        numeroEnsayo++;
        nivelEstimulo = !nivelEstimulo;

        portENTER_CRITICAL(&muxLatencia);
        valorDedoAntes = ultimoValorDedo;
        llegoEscalon = false;
        ensayoArmado = true;
        portEXIT_CRITICAL(&muxLatencia);

        t0Ms = millis();
        t0Us = micros();
        digitalWrite(PIN_ESTIMULO, nivelEstimulo ? HIGH : LOW);
        estadoEnsayo = ENSAYO_MIDIENDO;
    }

    vTaskDelay(1);
}
