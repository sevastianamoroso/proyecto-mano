# Graficador MATLAB: funcionamiento

El archivo `graficador_Joaco.m` abre dos puertos serie a `115200` baudios:

- **TX:** el ESP32 del guante/transmisor.
- **RX:** el ESP32 de la mano/receptor.

Al iniciarse, pide los dos puertos COM disponibles. El control de flujo está deshabilitado y se
intenta bajar DTR y RTS para evitar que el monitor afecte a las placas.

## Telemetría a 50 Hz

En la rama `exp/medicion-latencia`, al compilar ambos firmwares con
`TELEMETRIA_MATLAB = 1`, las placas envían CSV.

```text
T,millis,secuencia,dedo1,dedo2,dedo3,dedo4,dedo5,muniecaVertical,muniecaRotacional
R,millis,secuencia,nuevo,estado,dedo1,dedo2,dedo3,dedo4,dedo5,muniecaVertical,muniecaRotacional
```

La trama `T` contiene la orden enviada por el guante. La trama `R` contiene la posición aplicada
por el receptor. Cuando `nuevo` vale `1`, el receptor procesó una trama nueva.

El graficador guarda las últimas tramas TX y busca la misma `secuencia` cuando llega una trama RX.
Entonces coloca la orden TX en el instante del reloj del receptor en que esa trama fue procesada.
Así las dos gráficas quedan alineadas y permiten comparar la orden con la respuesta mecánica.

## Modo legado a 5 Hz

Si `TELEMETRIA_MATLAB` está en `0`, el firmware sigue imprimiendo el texto de consola habitual.
El graficador lo reconoce y grafica sus siete señales, pero muestra el aviso **Modo legado**. En
este formato no existe una secuencia útil para alinear cada par TX/RX.

## Interfaz

- **Pausar/Reanudar:** detiene o reinicia la actualización visual.
- **Limpiar:** borra la historia del gráfico y reinicia la alineación.
- **Capturar PNG:** guarda la ventana como imagen.
- **Señales visibles:** permite ocultar o mostrar cada dedo y las dos muñecas.

La ventana visible es de cinco segundos y el eje de tiempo está expresado en milisegundos.

## Sobre la latencia

El graficador **no muestra una latencia numérica**. El instante en que MATLAB recibe una línea por
USB no equivale al instante en que se transmitió por ESP-NOW: incluye colas del USB, impresión
serie y la planificación de ambos ESP32. La asociación por secuencia se usa sólo para alinear las
curvas correctamente.

La latencia física de radio se mide con el mecanismo de GPIO y osciloscopio que ya incluye la rama
`exp/medicion-latencia`.
