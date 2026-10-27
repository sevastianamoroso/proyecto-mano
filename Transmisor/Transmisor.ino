// Transmisor.ino - firmware del guante.
//
// Lee los 5 potenciómetros de los dedos y el MPU6050 de la muñeca, calcula los
// ángulos que tiene que copiar la mano y se los manda al receptor por ESP-NOW,
// 50 veces por segundo. Toda la calibración se hace de este lado: la mano
// recibe ángulos de servo listos para usar.
//
// Para recalibrar los dedos: mandar 'c' por el monitor serie (115200 baudios).

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_idf_version.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Preferences.h>

#include "ProtocoloBrazo.h"

// ---------- Pines y parámetros ----------

// Los potes van a pines del ADC1 porque el ADC2 no se puede usar con el WiFi prendido
const int PIN_PULGAR  = 39; // SENSOR_VN
const int PIN_INDICE  = 34;
const int PIN_MEDIO   = 35;
const int PIN_ANULAR  = 32;
const int PIN_MENIQUE = 33;

const int PINES_DEDOS[TOTAL_DEDOS] = {
    PIN_PULGAR, PIN_INDICE, PIN_MEDIO, PIN_ANULAR, PIN_MENIQUE
};

// I2C del MPU6050
const int PIN_I2C_SDA = 21;
const int PIN_I2C_SCL = 22;

// Período del ciclo: 20 ms = 50 Hz
const TickType_t PERIODO_CONTROL_MS = 20;

// Cuánto le creemos al giróscopo en el filtro complementario (lo usual es 0.96 a 0.98)
const float ALPHA_COMPLEMENTARIO = 0.98f;
// Por si el MPU queda torcido en el guante: lo que marca con la mano derecha, en grados
const float TRIM_PITCH_DEG = 0.0f;
const float TRIM_ROLL_DEG  = 0.0f;
// Hasta dónde movemos la mano (°) y hasta dónde llegan los servos de la muñeca (°)
const float PITCH_MANO_ARRIBA = 45.0f;   // corresponde a SERVO_V_MIN
const float PITCH_MANO_ABAJO  = -25.0f;  // corresponde a SERVO_V_MAX
const float SERVO_V_MIN = 25.0f, SERVO_V_NEUTRO = 57.0f, SERVO_V_MAX = 90.0f;
const float ROLL_MANO_MAX = 90.0f;       // ±90° de mano son 0 a 180° de servo

// MAC del ESP32 de la mano. Si se cambia la placa hay que poner la nueva
// (se ve con WiFi.macAddress() en el receptor).
uint8_t macReceptor[] = {0x1C, 0xC3, 0xAB, 0xD2, 0x14, 0x9C};

// ---------- Variables globales ----------

Adafruit_MPU6050 mpu;
Preferences memoriaNVS;

MensajeBrazo paqueteSalida;
esp_now_peer_info_t infoReceptor;

// Lectura del ADC con la mano abierta (min) y cerrada (max), una por dedo.
// Estos son valores por defecto; los reales se cargan de la flash al arrancar.
uint16_t calMinADC[TOTAL_DEDOS] = {1200, 1200, 1200, 1200, 1200};
uint16_t calMaxADC[TOTAL_DEDOS] = {3200, 3200, 3200, 3200, 3200};

// Ángulos de la muñeca que va actualizando el filtro (°, respecto de la gravedad)
float anguloPitch = 0.0f;
float anguloRoll  = 0.0f;
// Lo que marca el giróscopo quieto (rad/s); se lo restamos a cada lectura
float gyroBiasX = 0.0f;
float gyroBiasY = 0.0f;
uint32_t tiempoAnteriorMicros = 0;
uint8_t contadorSecuencia = 0;

// Lo escribe el callback de envío; sólo se usa para mostrarlo por serie
volatile bool ultimoEnvioExitoso = false;
// En true mientras se calibran los dedos: la tarea no manda nada ni imprime
volatile bool calibrando = false;

void rutinaRecuperacionI2C(int pinSDA, int pinSCL);
void cargarCalibracionNVS();
void guardarCalibracionNVS();
void ejecutarCalibracionInteractiva();
uint16_t leerADCSobremuestreado(int pin, uint8_t muestras = 16);
void tareaTransmision(void* pvParameters);
void calibrarIMU();

// Pitch y roll a partir del acelerómetro, en grados. Como son cocientes,
// da lo mismo si la librería entrega m/s² o g.
static inline float rollAcc(const sensors_event_t& a) {
    return atan2(a.acceleration.y, a.acceleration.z) * RAD_TO_DEG;
}
static inline float pitchAcc(const sensors_event_t& a) {
    return atan2(-a.acceleration.x,
                 sqrt(a.acceleration.y * a.acceleration.y +
                      a.acceleration.z * a.acceleration.z)) * RAD_TO_DEG;
}

// ---------- Callback de ESP-NOW ----------

// Avisa si el receptor confirmó el último paquete. La firma cambió en el
// ESP-IDF 5, por eso hay dos versiones.
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
void OnDataSent(const wifi_tx_info_t* info, esp_now_send_status_t status) {
    ultimoEnvioExitoso = (status == ESP_NOW_SEND_SUCCESS);
}
#else
void OnDataSent(const uint8_t* mac_addr, esp_now_send_status_t status) {
    ultimoEnvioExitoso = (status == ESP_NOW_SEND_SUCCESS);
}
#endif

// ---------- Destrabar el bus I2C ----------

// Si el ESP32 se resetea en medio de una lectura, el MPU puede quedar tirando
// SDA a masa esperando más clocks, y el bus queda colgado. Mandando 9 pulsos
// en SCL y un STOP el sensor termina lo que estaba haciendo y suelta la línea.
void rutinaRecuperacionI2C(int pinSDA, int pinSCL) {
    pinMode(pinSDA, INPUT_PULLUP);
    pinMode(pinSCL, OUTPUT);

    for (int i = 0; i < 9; i++) {
        digitalWrite(pinSCL, LOW);
        delayMicroseconds(5);
        digitalWrite(pinSCL, HIGH);
        delayMicroseconds(5);
    }

    // STOP: SDA sube mientras SCL está en alto
    pinMode(pinSDA, OUTPUT);
    digitalWrite(pinSDA, LOW);
    delayMicroseconds(5);
    digitalWrite(pinSCL, HIGH);
    delayMicroseconds(5);
    digitalWrite(pinSDA, HIGH);
    delayMicroseconds(5);

    pinMode(pinSDA, INPUT_PULLUP);
    pinMode(pinSCL, INPUT_PULLUP);
}

// ---------- Calibración de los dedos (guardada en flash) ----------

// Se guarda en la NVS para no tener que calibrar cada vez que se prende el guante
void cargarCalibracionNVS() {
    memoriaNVS.begin("calib_brazo", true); // true = sólo lectura
    for (int i = 0; i < TOTAL_DEDOS; i++) {
        String kMin = "min_" + String(i);
        String kMax = "max_" + String(i);
        calMinADC[i] = memoriaNVS.getUShort(kMin.c_str(), 1200);
        calMaxADC[i] = memoriaNVS.getUShort(kMax.c_str(), 3200);
    }
    memoriaNVS.end();
    Serial.println("[NVS] Calibración cargada satisfactoriamente.");
}

void guardarCalibracionNVS() {
    memoriaNVS.begin("calib_brazo", false);
    for (int i = 0; i < TOTAL_DEDOS; i++) {
        String kMin = "min_" + String(i);
        String kMax = "max_" + String(i);
        memoriaNVS.putUShort(kMin.c_str(), calMinADC[i]);
        memoriaNVS.putUShort(kMax.c_str(), calMaxADC[i]);
    }
    memoriaNVS.end();
    Serial.println("[NVS] Nuevos valores de calibración almacenados.");
}

// Se pide abrir la mano, se lee; se pide cerrarla, se lee, y se guarda.
void ejecutarCalibracionInteractiva() {
    // Cuando loop() llega acá, la tarea de transmisión (que tiene más prioridad y
    // corre en el mismo núcleo) ya terminó su ciclo. Desde el siguiente deja de
    // leer los dedos y de transmitir, así que la mano se va sola a la posición
    // de reposo por el failsafe del receptor.
    calibrando = true;
    Serial.println("\n=== INICIANDO RUTINA DE CALIBRACIÓN BI-PUNTO ===");
    Serial.println("Transmisión y telemetría PAUSADAS (la mano robot irá a Home por failsafe).");
    Serial.println("Paso 1: Abra completamente la mano.");
    Serial.println("Enviando comando... Ingrese cualquier caracter y presione ENTER para registrar MIN.");
    while (Serial.available() == 0) { delay(50); }
    while (Serial.available() > 0) { Serial.read(); } // vacía lo que haya quedado (el ENTER)

    for (int i = 0; i < TOTAL_DEDOS; i++) {
        calMinADC[i] = leerADCSobremuestreado(PINES_DEDOS[i], 32);
        Serial.printf("Dedo %d - Minimo Registrado: %u\n", i, calMinADC[i]);
    }

    Serial.println("\nPaso 2: Cierre completamente la mano en puño.");
    Serial.println("Ingrese cualquier caracter y presione ENTER para registrar MAX.");
    while (Serial.available() == 0) { delay(50); }
    while (Serial.available() > 0) { Serial.read(); }

    for (int i = 0; i < TOTAL_DEDOS; i++) {
        calMaxADC[i] = leerADCSobremuestreado(PINES_DEDOS[i], 32);
        Serial.printf("Dedo %d - Maximo Registrado: %u\n", i, calMaxADC[i]);
    }

    guardarCalibracionNVS();
    Serial.println("=== CALIBRACIÓN COMPLETADA CON ÉXITO ===\n");
    calibrando = false;
}

// ---------- Lectura del ADC ----------

// El ADC del ESP32 es bastante ruidoso, así que promediamos varias lecturas seguidas
uint16_t leerADCSobremuestreado(int pin, uint8_t muestras) {
    uint32_t acumulador = 0;
    for (uint8_t i = 0; i < muestras; i++) {
        acumulador += analogRead(pin);
        delayMicroseconds(50);
    }
    return (uint16_t)(acumulador / muestras);
}

// ---------- Calibración del MPU6050 ----------

// Con la mano quieta (~1,5 s) promedia el giróscopo para sacar su offset. De paso
// arranca el filtro con el ángulo real del acelerómetro, así no sale desde 0°
// y tarda en llegar.
void calibrarIMU() {
    Serial.println("[MPU6050] Calibrando offsets: mantener la mano QUIETA...");
    const int N = 500;
    float sumGx = 0, sumGy = 0, sumPitch = 0, sumRoll = 0;
    sensors_event_t a, g, temp;
    for (int i = 0; i < N; i++) {
        mpu.getEvent(&a, &g, &temp);
        sumGx += g.gyro.x;
        sumGy += g.gyro.y;
        sumPitch += pitchAcc(a);
        sumRoll  += rollAcc(a);
        delay(3);
    }
    gyroBiasX   = sumGx / N;
    gyroBiasY   = sumGy / N;
    anguloPitch = sumPitch / N;
    anguloRoll  = sumRoll / N;
    Serial.printf("[MPU6050] Bias gyro X:%.4f Y:%.4f rad/s | Inicial P:%.1f R:%.1f°\n",
                  gyroBiasX, gyroBiasY, anguloPitch, anguloRoll);
}

// ---------- setup ----------

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n[SISTEMA] Iniciando Transmisor de Telerrobótica...");

    // ADC
    for (int i = 0; i < TOTAL_DEDOS; i++) {
        pinMode(PINES_DEDOS[i], INPUT);
    }
    analogReadResolution(12);       // 0 a 4095
    analogSetAttenuation(ADC_11db); // para poder leer hasta ~3,3 V

    cargarCalibracionNVS();

    // I2C: primero lo destrabamos por las dudas y después lo arrancamos a 400 kHz
    rutinaRecuperacionI2C(PIN_I2C_SDA, PIN_I2C_SCL);
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000);

    // MPU6050: a veces no contesta al primer intento, así que probamos hasta 5 veces
    bool mpuOk = false;
    for (int intento = 1; intento <= 5; intento++) {
        Serial.printf("[MPU6050] Intento de conexión %d/5...\n", intento);
        if (mpu.begin(0x68, &Wire)) {
            mpuOk = true;
            break;
        }
        delay(200);
    }

    if (!mpuOk) {
        Serial.println("[ERROR CRÍTICO] MPU6050 no detectado en I2C. Reintentando en bucle no bloqueante...");
    } else {
        mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
        mpu.setGyroRange(MPU6050_RANGE_500_DEG);
        // 44 Hz agrega ~4,9 ms de retardo; antes usábamos 21 Hz, que agrega ~8,5 ms
        mpu.setFilterBandwidth(MPU6050_BAND_44_HZ);
        Serial.println("[MPU6050] Configurado exitosamente (±2G, 500°/s, DLPF 44Hz).");
        calibrarIMU();
    }

    // WiFi y ESP-NOW
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false); // con el ahorro de energía activado la radio agrega retardo

    if (esp_now_init() != ESP_OK) {
        Serial.println("[ERROR CRÍTICO] Error al inicializar ESP-NOW.");
        return;
    }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    esp_now_register_send_cb(OnDataSent);
#else
    esp_now_register_send_cb((esp_now_send_cb_t)OnDataSent);
#endif

    // Registramos la mano como destino
    memcpy(infoReceptor.peer_addr, macReceptor, 6);
    infoReceptor.channel = 0;     // 0 = el canal en el que ya está el WiFi
    infoReceptor.encrypt = false; // sin cifrado: no hace falta y es más rápido

    if (esp_now_add_peer(&infoReceptor) != ESP_OK) {
        Serial.println("[ALERTA] Falló el registro del receptor como peer.");
    } else {
        Serial.println("[ESP-NOW] Receptor emparejado correctamente.");
    }

    // Punto de partida para el dt del filtro
    tiempoAnteriorMicros = micros();

    // La tarea va en el núcleo 1; el WiFi y ESP-NOW corren en el 0
    xTaskCreatePinnedToCore(
        tareaTransmision,
        "TaskTxBrazo",
        4096,
        NULL,
        2,    // más prioridad que loop(), que corre con 1
        NULL,
        1
    );

    Serial.println("[SISTEMA] Tarea de transmisión iniciada a 50 Hz.");
    Serial.println("[SISTEMA] Envíe 'c' por el monitor serial para calibrar potenciómetros.\n");
}

// ---------- Tarea principal: leer, calcular y mandar ----------

void tareaTransmision(void* pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    uint32_t ticksTelemetria = 0;

    for (;;) {
        // vTaskDelayUntil (y no vTaskDelay) para que el período sea siempre 20 ms,
        // tarde lo que tarde el resto del ciclo
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(PERIODO_CONTROL_MS));

        // --- 1. Muñeca: MPU6050 + filtro complementario ---
        sensors_event_t a, g, temp;
        if (mpu.getEvent(&a, &g, &temp)) {
            uint32_t tiempoActualMicros = micros();
            float dt = (float)(tiempoActualMicros - tiempoAnteriorMicros) / 1000000.0f;
            tiempoAnteriorMicros = tiempoActualMicros;

            // Si el dt sale raro (primer ciclo, micros() que dio la vuelta) usamos el nominal
            if (dt <= 0.0f || dt > 0.1f) dt = 0.02f;

            // Velocidad de giro sin el offset, en °/s. Los signos están elegidos para
            // que coincidan con pitchAcc/rollAcc: +gyro.y sube el pitch, +gyro.x sube el roll.
            float gyroRatePitch = (g.gyro.y - gyroBiasY) * RAD_TO_DEG;
            float gyroRateRoll  = (g.gyro.x - gyroBiasX) * RAD_TO_DEG;

            // El giróscopo responde rápido pero deriva; el acelerómetro no deriva pero
            // es ruidoso. Mezclando los dos nos quedamos con lo bueno de cada uno.
            anguloPitch = ALPHA_COMPLEMENTARIO * (anguloPitch + gyroRatePitch * dt)
                        + (1.0f - ALPHA_COMPLEMENTARIO) * pitchAcc(a);

            // Cerca de ±90° de pitch el roll del acelerómetro es indeterminado
            // (gimbal lock), así que dejamos el último valor bueno
            if (fabs(anguloPitch) < 80.0f) {
                anguloRoll = ALPHA_COMPLEMENTARIO * (anguloRoll + gyroRateRoll * dt)
                           + (1.0f - ALPHA_COMPLEMENTARIO) * rollAcc(a);
            }

            // Ángulo de la mano -> ángulo del servo.
            // Pitch: dos rectas que se juntan en SERVO_V_NEUTRO (mano derecha = 0°),
            // porque para arriba y para abajo el rango no es el mismo
            float p = constrain(anguloPitch - TRIM_PITCH_DEG, PITCH_MANO_ABAJO, PITCH_MANO_ARRIBA);
            float servoV = (p >= 0.0f)
                ? SERVO_V_NEUTRO - (p / PITCH_MANO_ARRIBA) * (SERVO_V_NEUTRO - SERVO_V_MIN)
                : SERVO_V_NEUTRO + (p / PITCH_MANO_ABAJO)  * (SERVO_V_MAX - SERVO_V_NEUTRO);
            paqueteSalida.muniecaVertical = (int16_t)lroundf(constrain(servoV, SERVO_V_MIN, SERVO_V_MAX));

            // Roll: corrimiento directo, -90..+90° de mano a 0..180° de servo
            float r = constrain(anguloRoll - TRIM_ROLL_DEG, -ROLL_MANO_MAX, ROLL_MANO_MAX);
            paqueteSalida.muniecaRotacional = (int16_t)lroundf(constrain(r + 90.0f, 0.0f, 180.0f));
        }

        // Mientras se calibra, el ADC lo usa la rutina de calibración, así que acá
        // no se lee ni se manda nada. El MPU sí se sigue leyendo para que el filtro
        // no pierda el ángulo.
        if (calibrando) continue;

        // --- 2. Dedos: ADC -> ángulo de servo (25 a 90°) ---
        for (int i = 0; i < TOTAL_DEDOS; i++) {
            uint16_t adcRaw = leerADCSobremuestreado(PINES_DEDOS[i], 16);

            int16_t anguloDedo;
            if (calMaxADC[i] > calMinADC[i]) {
                anguloDedo = map(adcRaw, calMinADC[i], calMaxADC[i], 25, 90);
            } else {
                // Pote montado al revés: el valor baja al cerrar la mano
                anguloDedo = map(adcRaw, calMaxADC[i], calMinADC[i], 90, 25);
            }
            paqueteSalida.anguloDedos[i] = (uint16_t)constrain(anguloDedo, 25, 90);
        }

        // --- 3. Armado del paquete ---
        paqueteSalida.magicHeader = PROTOCOLO_HEADER_MAGIC;
        paqueteSalida.secuencia   = contadorSecuencia++;
        paqueteSalida.checksum    = calcularChecksumBrazo(paqueteSalida);

        // --- 4. Envío ---
        esp_now_send(macReceptor, reinterpret_cast<uint8_t*>(&paqueteSalida), sizeof(paqueteSalida));

        // --- 5. Telemetría por serie, 1 de cada 10 ciclos (5 Hz) para no frenar la tarea ---
        if (++ticksTelemetria >= 10) {
            ticksTelemetria = 0;
            Serial.printf("[TX #%u] Dedos:[%u, %u, %u, %u, %u] | Mñc:[V:%d, R:%d] | RF:%s\n",
                          paqueteSalida.secuencia,
                          paqueteSalida.anguloDedos[0],
                          paqueteSalida.anguloDedos[1],
                          paqueteSalida.anguloDedos[2],
                          paqueteSalida.anguloDedos[3],
                          paqueteSalida.anguloDedos[4],
                          paqueteSalida.muniecaVertical,
                          paqueteSalida.muniecaRotacional,
                          ultimoEnvioExitoso ? "OK" : "NO_ACK");
        }
    }
}

// ---------- loop ----------

// Todo el trabajo lo hace la tarea; loop() sólo espera la 'c' para calibrar
void loop() {
    if (Serial.available() > 0) {
        char cmd = Serial.read();
        if (cmd == 'c' || cmd == 'C') {
            ejecutarCalibracionInteractiva();
        }
    }
    vTaskDelay(pdMS_TO_TICKS(100));
}
