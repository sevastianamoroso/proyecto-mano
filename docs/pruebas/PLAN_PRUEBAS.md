# Plan de pruebas – Muñeca (MPU6050) y sistema completo

Objetivo: comparar `v1.0.0` (Kalman) contra `v2.0.0` (complementario) con mediciones repetibles, y
validar el sistema completo antes de seguir agregando funciones.

Cada prueba se anota en [`REGISTRO_PRUEBAS.md`](REGISTRO_PRUEBAS.md) con versión, fecha y resultado.

> **Seguridad:** las primeras pruebas de muñeca se hacen **con los servos de la muñeca desconectados
> mecánicamente** (o sin la mano montada), mirando sólo la telemetría serie. Recién cuando los valores
> estén dentro de rango se acopla la mecánica.

## Material

- Monitor serie a 115200 baudios (el Transmisor imprime `Mñc:[V:.., R:..]` a 5 Hz, en grados de servo).
- Transportador o app de inclinómetro en el celular, apoyado sobre el guante, para tener ángulos de referencia.
- Superficie plana y estable.
- Cronómetro.

## Bloque A – Arranque (sólo Transmisor)

| ID | Prueba | Procedimiento | Criterio de aceptación |
|---|---|---|---|
| A1 | Compilación | Compilar Transmisor y Receptor de cada versión. | Compila sin errores ni warnings nuevos. |
| A2 | Calibración inicial (v2) | Encender con el guante quieto sobre la mesa. | Aparece `[MPU6050] Bias gyro...` con valores pequeños (del orden de 0.01 rad/s) y el ángulo inicial coincide con la pose. |
| A3 | Calibración con movimiento (v2) | Encender moviendo el guante. | Se documenta el efecto (esperable: deriva fija). Confirma la necesidad de la mejora M4. |

## Bloque B – IMU, comparación v1 vs v2 (el problema "tipo mouse")

Hacer cada prueba con **v1.0.0 y con v2.0.0**, en las mismas condiciones.

| ID | Prueba | Procedimiento | Criterio de aceptación (v2) |
|---|---|---|---|
| B1 | Deriva en reposo | Guante quieto 5 min. Anotar V y R cada 30 s. | Variación total ≤ ±1° de servo. |
| B2 | Retorno a la posición (prueba clave) | Partir de pose neutra, inclinar a ~30°, sostener 10 s y volver a neutra. Repetir 10 veces. Anotar el valor tras cada vuelta. | El valor en neutra se repite con ±2° y no "camina" entre repeticiones. |
| B3 | Ángulos conocidos | Con el inclinómetro: Pitch a −25, −15, 0, +15, +30, +45° y Roll a −90, −45, 0, +45, +90°. | Relación monótona y repetible. Con los datos se ajustan `TRIM_*` y los rangos del mapeo. |
| B4 | Movimiento rápido | Giros bruscos y regreso. | Sin sobreoscilación visible, se asienta en < 1 s. |
| B5 | Singularidad | Llevar el Pitch a ~90° (mano vertical) y girar. | El Roll se mantiene (no salta) y se recupera al bajar. |
| B6 | Aceleración lineal | Desplazar el guante en línea recta sin rotarlo (adelante/atrás, arriba/abajo). | Perturbación pequeña y transitoria. Si es grande, priorizar la mejora M6. |
| B7 | Sentido de giro | Inclinar la mano hacia cada lado. | El valor de servo se mueve en el sentido esperado para cada eje. |

## Bloque C – Mano completa (Receptor + mecánica)

| ID | Prueba | Procedimiento | Criterio de aceptación |
|---|---|---|---|
| C1 | Límites mecánicos | Con el Transmisor apagado, comandar a mano (o forzar valores) los extremos 25/90 (vertical) y 0/180 (rotación). | Ninguna pieza 3D golpea ni se fuerza. Si golpea, ajustar los `LIM_*` del Receptor y los rangos del Transmisor. |
| C2 | Seguimiento de muñeca | Repetir B2 con la mano montada. | La mano vuelve a la misma posición en cada repetición. |
| C3 | Dedos | Calibrar con `c` (mano abierta/cerrada) y abrir/cerrar 10 veces. | Recorrido completo, sin zumbido en reposo. |
| C4 | Failsafe | Apagar el Transmisor durante la operación. | En ~250 ms la mano va suave a Home y a los ~2 s se desacoplan los servos. Al volver a encender, se re-sincroniza. |
| C5 | Alcance / pérdida RF | Alejarse progresivamente y con obstáculos. | Anotar la distancia a la que aparece `NO_ACK` o entra el failsafe. |
| C6 | Resistencia | 30 min de uso continuo. | Sin reinicios ni *brownouts*; temperatura de servos y regulador tolerable al tacto. Anotar si algún servo calienta. |
| C7 | Alimentación | Mover todos los servos a la vez (cerrar la mano y girar la muñeca). | El ESP32 Receptor no se reinicia. Si se reinicia, revisar la fuente y los capacitores antes de seguir. |

## Flujo de trabajo con git durante las pruebas

1. Flashear la versión a probar (`git checkout v1.0.0` / `v2.0.0`).
2. Anotar los resultados en `REGISTRO_PRUEBAS.md` (en `main`).
3. Cada cambio de firmware que se quiera probar va en su propia rama (`exp/<nombre>`, p. ej. `exp/alpha-096`).
   Si funciona, se integra en `main` y se etiqueta una versión nueva (`v2.1.0`, ...).
4. Hacer commit y push al final de cada sesión, para no perder resultados.

---

# Backlog de mejoras de firmware

Ordenado por prioridad: lo primero facilita medir; después viene robustez y por último lo exploratorio.

| ID | Mejora | Por qué | Esfuerzo |
|---|---|---|---|
| M1 | **Telemetría CSV para el Serial Plotter** (Pitch/Roll del acelerómetro, filtrados, valores de servo), activable con un comando serie. | Hoy sólo se ven los grados de servo a 5 Hz: no se ve el ángulo real ni la acción del filtro. Es lo que más ayuda a medir B1–B6. | Bajo |
| M2 | **Ajuste en caliente por serie** de `ALPHA`, `TRIM_PITCH`, `TRIM_ROLL`, guardado en NVS (ya se usa `Preferences`). | Probar α = 0.96/0.97/0.98 y ajustar el trim sin reflashear. | Bajo |
| M3 | **Comando "cero"**: con la mano en pose neutra, capturar los ángulos actuales como trim y guardarlos en NVS. | Calibración de montaje en 2 segundos, en vez de editar el código. | Bajo |
| M4 | **Validar la calibración del gyro**: si la varianza de las muestras es alta (la mano se movió), repetir la calibración y avisar. | Evita el error fijo de la prueba A3. | Bajo |
| M5 | **Reintento del MPU6050 en ejecución**: hoy, si falla al inicio, el mensaje dice "reintentando", pero el código no reintenta. Tampoco se detecta si se cae durante la operación. | Robustez del guante (cables que se mueven con la mano). | Medio |
| M6 | **Peso adaptativo del acelerómetro**: reducir la corrección del acelerómetro cuando \|a\| se aleja de 1 g (hay movimiento lineal). | Mejora la prueba B6. | Bajo |
| M7 | **Estadísticas de enlace en el Receptor**: contar saltos en `secuencia` (% de paquetes perdidos) e imprimirlo en la telemetría. | Datos objetivos para C5. | Bajo |
| M8 | **Recalibración del bias en reposo**: si el gyro está quieto N segundos, actualizar el bias lentamente. | El bias del MPU6050 cambia con la temperatura; ayuda en C6. | Medio |
| M9 | **Parámetros de la muñeca ajustables en el Receptor** (deadband, velocidad máxima) por serie. | Ajustar la suavidad sin reflashear. | Medio |
| M10 | **Comparar con el DMP interno del MPU6050** (librería I2Cdev/MPU6050_6Axis) como rama experimental. | Referencia contra la cual medir el filtro propio. Agrega una dependencia, por eso queda último. | Alto |

Notas:
- El *yaw* (giro sobre el eje vertical) no se puede medir de forma absoluta con el MPU6050 (no tiene magnetómetro). Si en algún momento hace falta, requiere otro sensor (p. ej. MPU9250/ICM-20948 o un magnetómetro aparte).
- `EXPLICACION_CODIGO.md` describe la fusión por Kalman de la v1.0.0. Conviene actualizarlo cuando la v2 quede validada.
