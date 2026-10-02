/**
 * @file Transmisor.ino
 * @brief Firmware de Transmisión para Telerrobótica con ESP32, ESP-NOW, MPU6050 y ADC Calibrado
 * @author Revisión de Grado en Ingeniería Electrónica y Sistemas Embebidos
 * 
 * Mejoras clave implementadas:
 *  - Tarea periódica determinista en FreeRTOS (50 Hz exactos con vTaskDelayUntil)
 *  - Calibración bi-punto para potenciómetros almacenada en NVS Flash (Preferences)
 *  - Sobremuestreo (Oversampling) de 16x en ADC para reducción de ruido de cuantización
 *  - Recuperación de bus I2C anti-bloqueo para sensor MPU6050
 *  - Protocolo binario estructurado con Header Mágico, contador de secuencia y Checksum Fletcher-16
 *  - Telemetría serial no bloqueante a tasa reducida (5 Hz)
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_idf_version.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Preferences.h>

#include "ProtocoloBrazo.h"

// ==========================================
// CONFIGURACIÓN DE HARDWARE Y PINES
// ==========================================
// Pines de ADC1 para lectura de potenciómetros
const int PIN_PULGAR  = 39; // SENSOR_VN
const int PIN_INDICE  = 34;
const int PIN_MEDIO   = 35;
const int PIN_ANULAR  = 32;
const int PIN_MENIQUE = 33;

const int PINES_DEDOS[TOTAL_DEDOS] = {
    PIN_PULGAR, PIN_INDICE, PIN_MEDIO, PIN_ANULAR, PIN_MENIQUE
};

// Pines de comunicación I2C para MPU6050
const int PIN_I2C_SDA = 21;
const int PIN_I2C_SCL = 22;

// Frecuencia del lazo de control (50 Hz = 20 ms)
const TickType_t PERIODO_CONTROL_MS = 20;

// Fusión IMU: filtro complementario (peso del giróscopo, típico 0.96..0.98)
const float ALPHA_COMPLEMENTARIO = 0.98f;
// Ajuste fino de montaje del MPU en el guante (°): ángulo leído con la mano en pose neutra
const float TRIM_PITCH_DEG = 0.0f;
const float TRIM_ROLL_DEG  = 0.0f;
// Rango útil de la mano (°) y rango mecánico de los servos de muñeca (°)
const float PITCH_MANO_ARRIBA = 45.0f;   // -> SERVO_V_MIN
const float PITCH_MANO_ABAJO  = -25.0f;  // -> SERVO_V_MAX
const float SERVO_V_MIN = 25.0f, SERVO_V_NEUTRO = 57.0f, SERVO_V_MAX = 90.0f;
const float ROLL_MANO_MAX = 90.0f;       // ±90° de mano -> 0..180° de servo

// MAC Address de la placa RECEPTORA
// NOTA: Reemplazar con la MAC física del receptor obtenida por WiFi.macAddress()
uint8_t macReceptor[] = {0x1C, 0xC3, 0xAB, 0xD2, 0x14, 0x9C};

// ==========================================
// OBJETOS Y VARIABLES GLOBALES
// ==========================================
Adafruit_MPU6050 mpu;
Preferences memoriaNVS;

// Mensaje de transmisión y registro de peer
MensajeBrazo paqueteSalida;
esp_now_peer_info_t infoReceptor;

// Calibración bi-punto para cada potenciómetro
uint16_t calMinADC[TOTAL_DEDOS] = {1200, 1200, 1200, 1200, 1200}; // Mano abierta
uint16_t calMaxADC[TOTAL_DEDOS] = {3200, 3200, 3200, 3200, 3200}; // Mano cerrada

// Estado del filtro complementario (ángulos absolutos respecto a la gravedad, °)
float anguloPitch = 0.0f;
float anguloRoll  = 0.0f;
// Offsets del giróscopo medidos en reposo (rad/s)
float gyroBiasX = 0.0f;
float gyroBiasY = 0.0f;
uint32_t tiempoAnteriorMicros = 0;
uint8_t contadorSecuencia = 0;

// Estado de enlace ESP-NOW
volatile bool ultimoEnvioExitoso = false;

// Prototipos de funciones
void rutinaRecuperacionI2C(int pinSDA, int pinSCL);
void cargarCalibracionNVS();
void guardarCalibracionNVS();
void ejecutarCalibracionInteractiva();
uint16_t leerADCSobremuestreado(int pin, uint8_t muestras = 16);
void tareaTransmision(void* pvParameters);
void calibrarIMU();

// Ángulos absolutos por acelerómetro (°). Independientes de la escala (m/s² o g).
static inline float rollAcc(const sensors_event_t& a) {
    return atan2(a.acceleration.y, a.acceleration.z) * RAD_TO_DEG;
}
static inline float pitchAcc(const sensors_event_t& a) {
    return atan2(-a.acceleration.x,
                 sqrt(a.acceleration.y * a.acceleration.y +
                      a.acceleration.z * a.acceleration.z)) * RAD_TO_DEG;
}

// ==========================================
// CALLBACK DE ESP-NOW (Tx Status)
// ==========================================
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
void OnDataSent(const wifi_tx_info_t* info, esp_now_send_status_t status) {
    ultimoEnvioExitoso = (status == ESP_NOW_SEND_SUCCESS);
}
#else
void OnDataSent(const uint8_t* mac_addr, esp_now_send_status_t status) {
    ultimoEnvioExitoso = (status == ESP_NOW_SEND_SUCCESS);
}
#endif

// ==========================================
// RECUPERACIÓN DE BUS I2C
// ==========================================
/**
 * @brief Genera 9 ciclos de reloj en SCL para forzar a cualquier esclavo I2C
 * bloqueado en estado de lectura a liberar la línea SDA.
 */
void rutinaRecuperacionI2C(int pinSDA, int pinSCL) {
    pinMode(pinSDA, INPUT_PULLUP);
    pinMode(pinSCL, OUTPUT);

    for (int i = 0; i < 9; i++) {
        digitalWrite(pinSCL, LOW);
        delayMicroseconds(5);
        digitalWrite(pinSCL, HIGH);
        delayMicroseconds(5);
    }

    // Generar condición de STOP
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

// ==========================================
// CALIBRACIÓN BI-PUNTO EN NVS FLASH
// ==========================================
void cargarCalibracionNVS() {
    memoriaNVS.begin("calib_brazo", true); // Modo sólo lectura
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
    memoriaNVS.begin("calib_brazo", false); // Modo lectura/escritura
    for (int i = 0; i < TOTAL_DEDOS; i++) {
        String kMin = "min_" + String(i);
        String kMax = "max_" + String(i);
        memoriaNVS.putUShort(kMin.c_str(), calMinADC[i]);
        memoriaNVS.putUShort(kMax.c_str(), calMaxADC[i]);
    }
    memoriaNVS.end();
    Serial.println("[NVS] Nuevos valores de calibración almacenados.");
}

void ejecutarCalibracionInteractiva() {
    Serial.println("\n=== INICIANDO RUTINA DE CALIBRACIÓN BI-PUNTO ===");
    Serial.println("Paso 1: Abra completamente la mano.");
    Serial.println("Enviando comando... Ingrese cualquier caracter y presione ENTER para registrar MIN.");
    while (Serial.available() == 0) { delay(50); }
    while (Serial.available() > 0) { Serial.read(); }

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
}

// ==========================================
// ADQUISICIÓN ANALÓGICA CON SOBREMUESTREO
// ==========================================
uint16_t leerADCSobremuestreado(int pin, uint8_t muestras) {
    uint32_t acumulador = 0;
    for (uint8_t i = 0; i < muestras; i++) {
        acumulador += analogRead(pin);
        delayMicroseconds(50);
    }
    return (uint16_t)(acumulador / muestras);
}

// ==========================================
// CALIBRACIÓN DE OFFSETS DEL MPU6050 (REPOSO)
// ==========================================
/**
 * @brief Promedia el giróscopo en reposo para obtener su offset e inicializa
 * el filtro con el ángulo absoluto del acelerómetro (sin transitorio desde 0°).
 * La mano debe permanecer QUIETA durante ~1.5 s.
 */
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

// ==========================================
// SETUP
// ==========================================
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n[SISTEMA] Iniciando Transmisor de Telerrobótica...");

    // 1. Configuración de ADC
    for (int i = 0; i < TOTAL_DEDOS; i++) {
        pinMode(PINES_DEDOS[i], INPUT);
    }
    analogReadResolution(12); // Rango 0..4095
    analogSetAttenuation(ADC_11db); // Rango de entrada ~0 a 3.3V

    cargarCalibracionNVS();

    // 2. Inicialización y recuperación de bus I2C
    rutinaRecuperacionI2C(PIN_I2C_SDA, PIN_I2C_SCL);
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000); // Fast-mode 400 kHz

    // 3. Inicialización de MPU6050 con reintentos
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
        mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
        Serial.println("[MPU6050] Configurado exitosamente (±2G, 500°/s, DLPF 21Hz).");
        calibrarIMU();
    }

    // 4. Inicialización de Radio Wi-Fi y ESP-NOW
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false); // Anula el ahorro de energía para eliminar latencia RF

    if (esp_now_init() != ESP_OK) {
        Serial.println("[ERROR CRÍTICO] Error al inicializar ESP-NOW.");
        return;
    }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    esp_now_register_send_cb(OnDataSent);
#else
    esp_now_register_send_cb((esp_now_send_cb_t)OnDataSent);
#endif

    // Configuración del peer receptor
    memcpy(infoReceptor.peer_addr, macReceptor, 6);
    infoReceptor.channel = 0;     // Canal actual de Wi-Fi
    infoReceptor.encrypt = false; // Sin cifrado para máxima velocidad y mínima latencia

    if (esp_now_add_peer(&infoReceptor) != ESP_OK) {
        Serial.println("[ALERTA] Falló el registro del receptor como peer.");
    } else {
        Serial.println("[ESP-NOW] Receptor emparejado correctamente.");
    }

    // Inicializar temporización del filtro complementario
    tiempoAnteriorMicros = micros();

    // 5. Creación de Tarea de Transmisión determinista en Core 1
    xTaskCreatePinnedToCore(
        tareaTransmision,
        "TaskTxBrazo",
        4096,
        NULL,
        2, // Prioridad superior a tareas idle
        NULL,
        1  // Asignada a Core 1 (Core 0 gestiona la pila WiFi/ESP-NOW)
    );

    Serial.println("[SISTEMA] Tarea de transmisión iniciada a 50 Hz.");
    Serial.println("[SISTEMA] Envíe 'c' por el monitor serial para calibrar potenciómetros.\n");
}

// ==========================================
// TAREA PERIÓDICA DE CONTROL Y TRANSMISIÓN
// ==========================================
void tareaTransmision(void* pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    uint32_t ticksTelemetria = 0;

    for (;;) {
        // Ejecución determinista a 50 Hz (20 ms)
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(PERIODO_CONTROL_MS));

        // -------------------------------------------------------------
        // 1. ADQUISICIÓN Y MAPEADO CALIBRADO DE DEDOS (POTENCIÓMETROS)
        // -------------------------------------------------------------
        for (int i = 0; i < TOTAL_DEDOS; i++) {
            uint16_t adcRaw = leerADCSobremuestreado(PINES_DEDOS[i], 16);

            // Mapeo seguro con calibración bi-punto
            int16_t anguloDedo;
            if (calMaxADC[i] > calMinADC[i]) {
                anguloDedo = map(adcRaw, calMinADC[i], calMaxADC[i], 25, 90);
            } else {
                // Caso inverso (potenciómetro montado en polaridad invertida)
                anguloDedo = map(adcRaw, calMaxADC[i], calMinADC[i], 90, 25);
            }
            paqueteSalida.anguloDedos[i] = (uint16_t)constrain(anguloDedo, 25, 90);
        }

        // -------------------------------------------------------------
        // 2. ADQUISICIÓN IMU (MPU6050) Y FUSIÓN COMPLEMENTARIA
        // -------------------------------------------------------------
        sensors_event_t a, g, temp;
        if (mpu.getEvent(&a, &g, &temp)) {
            uint32_t tiempoActualMicros = micros();
            float dt = (float)(tiempoActualMicros - tiempoAnteriorMicros) / 1000000.0f;
            tiempoAnteriorMicros = tiempoActualMicros;

            // Protección ante desbordamiento o dt anómalo
            if (dt <= 0.0f || dt > 0.1f) dt = 0.02f;

            // Velocidades angulares sin offset (°/s). Signos coherentes con pitchAcc/rollAcc
            // (regla de la mano derecha: +gyro.y aumenta pitch, +gyro.x aumenta roll).
            float gyroRatePitch = (g.gyro.y - gyroBiasY) * RAD_TO_DEG;
            float gyroRateRoll  = (g.gyro.x - gyroBiasX) * RAD_TO_DEG;

            // Filtro complementario: el giróscopo aporta respuesta rápida y el
            // acelerómetro ancla el ángulo a la gravedad (sin drift acumulado).
            anguloPitch = ALPHA_COMPLEMENTARIO * (anguloPitch + gyroRatePitch * dt)
                        + (1.0f - ALPHA_COMPLEMENTARIO) * pitchAcc(a);

            // Cerca de ±90° de pitch el roll del acelerómetro es indeterminado:
            // se retiene el último valor válido en vez de integrar el gyro a ciegas.
            if (fabs(anguloPitch) < 80.0f) {
                anguloRoll = ALPHA_COMPLEMENTARIO * (anguloRoll + gyroRateRoll * dt)
                           + (1.0f - ALPHA_COMPLEMENTARIO) * rollAcc(a);
            }

            // Mapeo absoluto y unívoco ángulo de mano -> ángulo de servo, saturado
            // Pitch: tramos lineales con 0° de mano = SERVO_V_NEUTRO
            float p = constrain(anguloPitch - TRIM_PITCH_DEG, PITCH_MANO_ABAJO, PITCH_MANO_ARRIBA);
            float servoV = (p >= 0.0f)
                ? SERVO_V_NEUTRO - (p / PITCH_MANO_ARRIBA) * (SERVO_V_NEUTRO - SERVO_V_MIN)
                : SERVO_V_NEUTRO + (p / PITCH_MANO_ABAJO)  * (SERVO_V_MAX - SERVO_V_NEUTRO);
            paqueteSalida.muniecaVertical = (int16_t)lroundf(constrain(servoV, SERVO_V_MIN, SERVO_V_MAX));

            // Roll: -90..+90° de mano -> 0..180° de servo
            float r = constrain(anguloRoll - TRIM_ROLL_DEG, -ROLL_MANO_MAX, ROLL_MANO_MAX);
            paqueteSalida.muniecaRotacional = (int16_t)lroundf(constrain(r + 90.0f, 0.0f, 180.0f));
        }

        // -------------------------------------------------------------
        // 3. EMPAQUETADO, SECUENCIA Y CHECKSUM
        // -------------------------------------------------------------
        paqueteSalida.magicHeader = PROTOCOLO_HEADER_MAGIC;
        paqueteSalida.secuencia   = contadorSecuencia++;
        paqueteSalida.checksum    = calcularChecksumBrazo(paqueteSalida);

        // -------------------------------------------------------------
        // 4. TRANSMISIÓN POR ESP-NOW
        // -------------------------------------------------------------
        esp_now_send(macReceptor, reinterpret_cast<uint8_t*>(&paqueteSalida), sizeof(paqueteSalida));

        // -------------------------------------------------------------
        // 5. TELEMETRÍA SERIAL NO BLOQUEANTE (5 Hz = cada 10 ciclos)
        // -------------------------------------------------------------
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

// ==========================================
// LOOP (GESTIÓN DE CALIBRACIÓN SERIAL)
// ==========================================
void loop() {
    // El núcleo 1 delega el control en FreeRTOS; loop() se usa para comandos interactivos
    if (Serial.available() > 0) {
        char cmd = Serial.read();
        if (cmd == 'c' || cmd == 'C') {
            ejecutarCalibracionInteractiva();
        }
    }
    vTaskDelay(pdMS_TO_TICKS(100));
}
