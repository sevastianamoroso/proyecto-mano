# Mano Robótica Telecomandada (ESP32 + ESP-NOW + MPU6050)

Proyecto final – DIEC, Universidad Nacional del Sur.
Autores: Joaquín Petruf, Sebastián Amoroso.

Un guante transmisor (5 potenciómetros + MPU6050) comanda por ESP-NOW una mano impresa en 3D
(5 servos SG90 en dedos + 2 servos MG946R en muñeca).

## Estructura

| Carpeta / archivo | Contenido |
|---|---|
| `Transmisor/` | Firmware del guante (`Transmisor.ino`), protocolo y diagrama de conexionado |
| `Receptor/` | Firmware de la mano (`Receptor.ino`), protocolo y diagrama de conexionado |
| `docs/pruebas/` | Plan de pruebas, registro de resultados y backlog de mejoras |
| `EXPLICACION_CODIGO.md` | Explicación del sistema (describe la fusión por Kalman de la v1.0.0) |
| `MEJORAS_HARDWARE_Y_FIRMWARE.md` | Notas de mejoras de hardware y firmware |

## Versiones

Cada versión es un *tag* de git. Para compilar una versión concreta:

```bash
git checkout v1.0.0   # o v2.0.0
# abrir Transmisor/Transmisor.ino y Receptor/Receptor.ino en el Arduino IDE y flashear
git checkout main     # volver a la última versión
```

| Versión | Fusión IMU (muñeca) | Estado |
|---|---|---|
| `v1.0.0` | Filtro de Kalman. Signo del gyro de Pitch invertido y uso de variable sin inicializar en Roll → la muñeca se comporta "como un mouse" (deriva/histéresis). | Original, referencia para comparar |
| `v2.0.0` | Filtro complementario (α = 0.98) sobre ángulos absolutos del acelerómetro, calibración de bias del gyro al inicio, mapeo saturado. Receptor con límites mecánicos (`constrain`). | A validar en hardware |

**Compatibilidad:** el protocolo (`ProtocoloBrazo.h`) es idéntico en ambas versiones, por lo que para
comparar la muñeca alcanza con reflashear sólo el **Transmisor**. El Receptor v2.0.0 sólo agrega saturación
de seguridad y puede quedar fijo.

## Pruebas

- Plan y criterios: [`docs/pruebas/PLAN_PRUEBAS.md`](docs/pruebas/PLAN_PRUEBAS.md)
- Resultados: [`docs/pruebas/REGISTRO_PRUEBAS.md`](docs/pruebas/REGISTRO_PRUEBAS.md)
