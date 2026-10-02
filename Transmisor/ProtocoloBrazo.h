#ifndef PROTOCOLO_BRAZO_H
#define PROTOCOLO_BRAZO_H

#include <Arduino.h>

// Cabecera mágica para verificar la validez y alineación del paquete
#define PROTOCOLO_HEADER_MAGIC 0xAA55

// Índices para el arreglo de dedos
enum DedoIndex {
    DEDO_PULGAR  = 0,
    DEDO_INDICE  = 1,
    DEDO_MEDIO   = 2,
    DEDO_ANULAR  = 3,
    DEDO_MENIQUE = 4,
    TOTAL_DEDOS  = 5
};

// Estructura de paquete con alineación de 1 byte sin padding
#pragma pack(push, 1)
struct MensajeBrazo {
    uint16_t magicHeader;        // Debe coincidir con PROTOCOLO_HEADER_MAGIC (0xAA55)
    uint8_t  secuencia;          // Contador monotónico incremental (0..255)
    uint16_t anguloDedos[5];     // Ángulos objetivos para cada dedo (grados 25..90)
    int16_t  muniecaVertical;    // Ángulo vertical (Pitch) en grados (25..90)
    int16_t  muniecaRotacional;  // Ángulo rotacional (Roll) en grados (0..180)
    uint16_t checksum;           // Checksum para verificación de integridad de datos
};
#pragma pack(pop)

/**
 * @brief Calcula el checksum (Fletcher-16 simplificado) de la estructura.
 * Se excluye el campo de checksum del cálculo.
 */
inline uint16_t calcularChecksumBrazo(const MensajeBrazo& msg) {
    const uint8_t* buffer = reinterpret_cast<const uint8_t*>(&msg);
    size_t longitud = sizeof(MensajeBrazo) - sizeof(msg.checksum);
    uint16_t sum1 = 0;
    uint16_t sum2 = 0;
    
    for (size_t i = 0; i < longitud; ++i) {
        sum1 = (sum1 + buffer[i]) % 255;
        sum2 = (sum2 + sum1) % 255;
    }
    return (sum2 << 8) | sum1;
}

/**
 * @brief Valida la cabecera y el checksum del paquete recibido.
 */
inline bool validarPaqueteBrazo(const MensajeBrazo& msg) {
    if (msg.magicHeader != PROTOCOLO_HEADER_MAGIC) {
        return false;
    }
    return (calcularChecksumBrazo(msg) == msg.checksum);
}

#endif // PROTOCOLO_BRAZO_H
