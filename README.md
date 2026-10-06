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
| `EXPLICACION_CODIGO.md` | Explicación detallada del firmware v2.3.0, módulo por módulo, con la evolución desde la v1.0.0 |
| `docs/DIAGRAMAS.md` | Funcionamiento en diagramas (Mermaid) |
| `MEJORAS_HARDWARE_Y_FIRMWARE.md` | Notas de mejoras de hardware y firmware |

## Versiones

Cada versión es un *tag* de git. Para compilar una versión concreta:

```bash
git checkout v2.2.0   # o cualquier otro tag de la tabla
# abrir Transmisor/Transmisor.ino y Receptor/Receptor.ino en el Arduino IDE y flashear
git checkout main     # volver a la última versión
```

| Versión | Fusión IMU (muñeca) | Estado |
|---|---|---|
| `v1.0.0` | Filtro de Kalman. Signo del gyro de Pitch invertido y uso de variable sin inicializar en Roll → la muñeca se comporta "como un mouse" (deriva/histéresis). | Original, referencia para comparar |
| `v2.0.0` | Filtro complementario (α = 0.98) sobre ángulos absolutos del acelerómetro, calibración de bias del gyro al inicio, mapeo saturado. Receptor con límites mecánicos (`constrain`). | Superada por v2.2.0 |
| `v2.1.0` | Receptor: slew-rate más rápido (dedos 8°/ciclo, muñeca 4/5°/ciclo), `INVERTIR_DEDO[]` por dedo, `WiFi.setSleep(false)`. | Superada por v2.2.0 |
| `v2.2.0` | Latencia: el Receptor actúa al llegar cada paquete (sin esperar el tick de 50 Hz) y DLPF del MPU6050 a 44 Hz en el Transmisor. | ✅ Probada en hardware: **versión definitiva** |
| `v2.3.0` | Transmisor: al calibrar con `c` se pausan la lectura de dedos, el envío y la telemetría (la mano va a Home por failsafe y se recupera al terminar). | A validar en hardware |

**Compatibilidad:** el protocolo (`ProtocoloBrazo.h`) es idéntico en todas las versiones, así que se
pueden mezclar placas de distintas versiones. Para reproducir una versión completa (en especial `v2.1.0` y
`v2.2.0`, que cambian el Receptor) hay que flashear **ambas** placas con el mismo tag.

## Pruebas

- Plan y criterios: [`docs/pruebas/PLAN_PRUEBAS.md`](docs/pruebas/PLAN_PRUEBAS.md)
- Resultados: [`docs/pruebas/REGISTRO_PRUEBAS.md`](docs/pruebas/REGISTRO_PRUEBAS.md)
