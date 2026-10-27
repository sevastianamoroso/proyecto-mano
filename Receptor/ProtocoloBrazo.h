// ProtocoloBrazo.h - el formato del paquete que viaja del guante a la mano.
//
// Hay una copia en cada carpeta (Transmisor, Receptor y Receptor_Latencia) porque
// el IDE de Arduino sólo compila lo que está en la carpeta del programa.
// Si se cambia algo, hay que cambiarlo en todas.

#ifndef PROTOCOLO_BRAZO_H
#define PROTOCOLO_BRAZO_H

#include <Arduino.h>

// Valor fijo al principio de cada paquete. Si no está, no es un paquete nuestro
// (o llegó corrido) y se descarta.
#define PROTOCOLO_HEADER_MAGIC 0xAA55

// Posición de cada dedo en el arreglo anguloDedos
enum DedoIndex {
    DEDO_PULGAR  = 0,
    DEDO_INDICE  = 1,
    DEDO_MEDIO   = 2,
    DEDO_ANULAR  = 3,
    DEDO_MENIQUE = 4,
    TOTAL_DEDOS  = 5
};

// pack(1) para que el compilador no meta bytes de relleno entre campos: así el
// paquete ocupa siempre 19 bytes y se puede mandar tal cual está en memoria.
#pragma pack(push, 1)
struct MensajeBrazo {
    uint16_t magicHeader;        // siempre PROTOCOLO_HEADER_MAGIC (0xAA55)
    uint8_t  secuencia;          // contador que sube de a 1 en cada envío (0..255 y vuelve a 0)
    uint16_t anguloDedos[5];     // ángulo de servo de cada dedo, 25..90°
    int16_t  muniecaVertical;    // servo de la muñeca vertical (pitch), 25..90°
    int16_t  muniecaRotacional;  // servo de la muñeca rotacional (roll), 0..180°
    uint16_t checksum;           // calculado con calcularChecksumBrazo()
};
#pragma pack(pop)

// Checksum tipo Fletcher-16 sobre todos los bytes del paquete menos el propio checksum.
// sum2 acumula a sum1, así que también detecta bytes que llegaron cambiados de lugar.
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

// true si el paquete tiene la cabecera correcta y el checksum da
inline bool validarPaqueteBrazo(const MensajeBrazo& msg) {
    if (msg.magicHeader != PROTOCOLO_HEADER_MAGIC) {
        return false;
    }
    return (calcularChecksumBrazo(msg) == msg.checksum);
}

#endif // PROTOCOLO_BRAZO_H
