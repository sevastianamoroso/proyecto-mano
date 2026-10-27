# Preguntas y respuestas para la defensa

Complementa las preguntas de práctica de los archivos 01 a 03 (FreeRTOS, ESP-NOW y código), sin
repetirlas. Acá van el proyecto en general, el hardware, las pruebas, las preguntas incómodas y el
conocimiento de base que puede salir aunque no esté en el código.

Conviene tapar la respuesta y contestar primero en voz alta. Los números salen del informe, del firmware
v2.3.0 y de `docs/pruebas/REGISTRO_PRUEBAS.md`. Lo que es conocimiento general (no medido por nosotros)
está marcado como tal.

---

## A. El proyecto en general

**1. ¿Qué hace el proyecto, en una frase?**
Un guante con cinco potenciómetros y una IMU mide la mano del operador y, por ESP-NOW, comanda una mano
impresa en 3D con siete servos (cinco dedos y dos ejes de muñeca) que copia el movimiento en tiempo real.

**2. ¿Para qué sirve? ¿Cuál es la motivación?**
Telerrobótica: que una persona opere a distancia en lugares peligrosos (sustancias tóxicas, material
radiactivo, temperaturas extremas). Nuestro proyecto es una demostración de bajo costo de la cadena
completa: capturar el movimiento, mandarlo sin cables y replicarlo de forma segura.

**3. ¿Qué movimientos replica y cuáles no?**
Replica la flexión de cada dedo por separado, la flexión/extensión de la muñeca (*pitch*, "muñeca
vertical") y la rotación de la mano (pronosupinación, *roll*). No replica el giro sobre el eje vertical
(*yaw*), la separación lateral de los dedos ni la fuerza.

**4. ¿Cuál es la decisión de diseño más importante?**
Que **el guante manda ángulos de servo ya calculados**, no lecturas crudas. Toda la calibración del
operador vive en el transmisor; la mano sólo valida, suaviza y mueve. Cambiar de operador = recalibrar el
guante, sin tocar la mano.

**5. ¿Por qué dos ESP32 y no un ESP32 y otra placa (Arduino, etc.)?**
ESP-NOW es un protocolo de Espressif: necesita radios ESP en las dos puntas. Además el ESP32 tiene dos
núcleos (radio en uno, control en el otro), ADC, PWM por hardware (LEDC) y FreeRTOS incluidos, a bajo
costo.

**6. ¿Cuál fue la mayor dificultad?**
La muñeca. Con el filtro de Kalman de la v1.0.0 se comportaba "como un mouse": derivaba y no volvía a la
misma posición. Las causas fueron errores de implementación (signo del giróscopo de *pitch* invertido,
variable sin inicializar en el *roll*, arranque desde 0°, truncado a entero antes del mapeo). Se pasó a
un filtro complementario, que tiene un solo parámetro y es más fácil de verificar.

**7. ¿Cómo evolucionó el firmware?**
Preliminar (EMA + `delay(20)`) → v1.0.0 (FreeRTOS, cola, checksum, limitador, NVS, Kalman) → v2.0.0
(complementario) → v2.1.0 (limitador más rápido, `INVERTIR_DEDO`) → v2.2.0 (menos latencia: el receptor
procesa al llegar el paquete y DLPF a 44 Hz) → v2.3.0 (pausa del envío al calibrar). Cada versión es un
*tag* de git y se probó en hardware.

**8. ¿Por qué sacaron el filtro EMA de la versión preliminar?**
Había dos EMA en cascada (guante y mano) y sus retardos se sumaban. Además el EMA se acerca al objetivo
con pasos cada vez más chicos, que caen dentro de la zona muerta del servo y lo hacen zumbar. Se reemplazó
por limitador de velocidad + banda muerta en el receptor.

---

## B. Hardware y alimentación

**9. ¿Por qué SG90 en los dedos y MG946R en la muñeca?**
Los dedos no hacen fuerza sobre objetos (no está en el alcance), así que alcanza con un servo chico y
liviano que entre en la palma (SG90, 1,8 kg·cm). La muñeca carga con el peso de toda la mano, por eso
MG946R: engranajes metálicos y 10,5 kg·cm a 4,8 V.

**10. ¿Cómo se alimenta el receptor?**
Dos alimentaciones con masa común:
- **Servos:** fuente de 12 V / 4 A → regulador *buck* MP1584 → 5 V a la placa base.
- **ESP32:** por USB (en las pruebas, la computadora; en un diseño final, un *powerbank*).

Se quitó el pin 5V del ESP32 para que el regulador no quede unido al USB de la computadora.

**11. ¿Por qué separar la alimentación del ESP32 de la de los servos?**
Cuando varios servos arrancan a la vez, la línea de 5 V puede caer unos milisegundos. Si el ESP32 tomara
de ahí, su regulador de 3,3 V perdería regulación y el detector de baja tensión (*brownout*) lo
reiniciaría. Separadas, un pico de los motores no resetea el microcontrolador.

**12. ¿Por qué tienen que estar unidas las masas?**
Porque el PWM que genera el ESP32 se mide respecto de su GND. Sin masa común, el servo no tiene una
referencia para leer el pulso.

**13. ¿Para qué están los capacitores?**
Dos pares de 470 µF / 35 V electrolítico + 100 nF cerámico:
- **A la salida (5 V):** el electrolítico entrega los picos cortos que el regulador no alcanza a seguir;
  el cerámico filtra el ruido de alta frecuencia de los motores y de la conmutación.
- **A la entrada (12 V):** entrega los pulsos de la conmutación, así la fuente ve una corriente más pareja.
  Medido: el pico de corriente que ve la fuente bajó de 0,38 A a 0,18 A; la media no cambió.

**14. ¿Por qué un regulador conmutado y no un lineal (7805, LM317)?**
Bajar de 12 V a 5 V con un lineal disipa la diferencia en calor: con 1 A serían unos 7 W en el regulador.
El *buck* convierte con mucha mejor eficiencia (conocimiento general; no medimos la eficiencia).

**15. ¿Cuánto consume la mano?**
A 12 V: 75–100 mA medios en reposo; ~0,21 A medios moviendo los dos ejes de la muñeca, con picos de
0,6–0,72 A; ~0,55–0,6 A con un servo bloqueado a mano. Siempre por debajo de 0,75 A, muy lejos de los 4 A
de la fuente y los 3 A del regulador.

**16. ¿Cómo midieron la corriente?**
Con un osciloscopio sobre una resistencia de 0,5 Ω en el cable negativo de la fuente (resistencia
*shunt*): I = V / 0,5 Ω, o sea 1 V en pantalla = 2 A. La salida de 5 V se midió directo sobre la línea
de los servos: 5,14–5,25 V eficaces en 57 adquisiciones.

**17. En reposo la corriente sale en pulsos, ¿es una falla?**
No. Con poca carga el MP1584 trabaja en modo ráfaga (*pulse-skipping*): conmuta de a ratos para mantener
la eficiencia. Con más carga los pulsos desaparecen y conmuta en forma continua, que es lo que vimos.

**18. ¿Y el corte de ~0,4 ms al final de la captura del servo bloqueado?**
Es el instante en que se soltó el servo: deja de pedir corriente y el regulador deja de conmutar un
momento hasta volver al régimen de reposo. No es una falla.

**19. ¿Por qué el anular está en el GPIO 23 y no en el 12?**
El GPIO 12 es un pin de *strapping*: al arrancar, su nivel define la tensión de la memoria flash. Con el
servo ahí, el dedo se movía con dificultad. Para diagnosticarlo cambiamos el **mismo servo** a otro pin
(GPIO 13 y después 23) y respondió mucho mejor: **la falla seguía al pin, no al servo ni a la mecánica**.
Se reasignó al GPIO 23, que no es de *strapping*. No determinamos el mecanismo exacto; la solución fue
evitar ese pin.

**20. ¿Cómo eligieron los pines del guante?**
Los potenciómetros van al **ADC1** (GPIO 39, 34, 35, 32, 33) porque el ADC2 no se puede usar con la
radio encendida. El MPU6050 va al I²C en 21 (SDA) y 22 (SCL), los pines por defecto.

**21. ¿Cómo se conectan los potenciómetros?**
Son de 10 kΩ lineales, entre 3,3 V y GND, con el cursor al ADC. Así la tensión nunca supera el rango de
entrada del ADC. Cada dedo tira de una tanza que hace girar una palanca en el eje del potenciómetro, y un
resorte la devuelve al abrir y la mantiene tensa (menos holgura).

**22. ¿Cómo diseñaron e imprimieron las piezas?**
En Solid Edge (CAD de Siemens): bocetos en un plano y extrusiones/vaciados. Se imprimió todo en una
Hellbot Hidra. Los dedos se mueven con bielas desde servos en la palma; la rotación usa un rodamiento de
20 mm de diámetro interior. El recorrido libre de las piezas se pensó algo mayor que el rango de software
(25–90°), para que el servo nunca fuerce el plástico.

---

## C. Sensores y fusión

**23. ¿Qué mide el MPU6050 y cómo se lee?**
Acelerómetro y giróscopo de tres ejes (IMU de 6 grados de libertad), 16 bits por eje, por I²C a 400 kHz
en la dirección 0x68. Configurado en ±2 g y ±500 °/s, con el filtro pasa-bajos interno (DLPF) en 44 Hz.

**24. ¿Por qué ±2 g y ±500 °/s?**
±2 g da la mejor resolución del acelerómetro (16384 LSB/g) y una mano no supera esa aceleración en uso
normal. ±500 °/s (65,5 LSB por °/s) cubre giros rápidos de muñeca sin saturar.

**25. ¿Por qué no miden el *yaw*?**
El acelerómetro mide la gravedad, y girar alrededor del eje vertical no cambia la gravedad: no hay
referencia absoluta. El giróscopo solo derivaría. Haría falta un magnetómetro (por ejemplo un MPU9250 o
ICM-20948). Está en trabajo futuro.

**26. ¿Qué es el *gimbal lock* y cómo lo manejan?**
Cerca de ±90° de *pitch*, el *roll* calculado con el acelerómetro queda indeterminado (Y y Z tienden a
cero en `atan2`). Si |pitch| ≥ 80°, el código congela el *roll* en el último valor bueno.

**27. ¿Qué significa α = 0,98 en el filtro complementario?**
En cada ciclo se toma 98 % de la integración del giróscopo y 2 % del ángulo del acelerómetro. Con
Δt = 20 ms da una constante de tiempo τ = α·Δt/(1−α) ≈ 0,98 s: para movimientos más rápidos que eso manda
el giróscopo; en tiempos más largos el acelerómetro corrige la deriva.

**28. ¿Por qué el pitch se mapea con dos rectas?**
Porque el rango de la muñeca del operador no es simétrico (45° hacia arriba, 25° hacia abajo) y el servo
tampoco alrededor de su neutro (57°). Dos tramos lineales que comparten el punto neutro hacen que la mano
horizontal quede siempre en 57°.

**29. ¿Qué pasa si el MPU queda torcido en el guante?**
Hay dos constantes de *trim* (`TRIM_PITCH_DEG`, `TRIM_ROLL_DEG`): se pone el ángulo que mide el sensor
con la mano en pose neutra y se resta antes de convertir. Hoy están en 0; un comando "cero" que las tome
solas está en el backlog (M3).

**30. Si el acelerómetro mide la gravedad, ¿qué pasa si muevo el guante sin rotarlo?**
La aceleración lineal se suma a la gravedad y el acelerómetro "ve" una inclinación falsa. El filtro la
atenúa mucho porque el acelerómetro sólo pesa un 2 %, así que es una perturbación chica y transitoria.
Bajar el peso del acelerómetro cuando |a| se aleja de 1 g es la mejora M6 del backlog.

---

## D. Comunicación, firmware y tiempo real

**31. ¿Cuál es la latencia medida?**
Unos **24 ms de media** de punta a punta (≈ 420 ensayos en bloques de 15; medias de bloque entre 20 y
35 ms). Equivale a uno o dos ciclos de 20 ms, coincide con la estimación (~30 ms) y es mucho menor que lo
que tarda el propio servo en moverse (~100 ms cada 60° en el SG90). El cuello de botella es la mecánica,
no el enlace.

**32. ¿Cómo la midieron sin sincronizar los relojes de las dos placas?**
El receptor genera un escalón de tensión que reemplaza al potenciómetro del índice en la entrada del
transmisor (GPIO 34) y mide, con **su propio reloj**, cuándo termina el primer pulso PWM con el ancho
nuevo en la salida del servo del índice. Inicio y fin se toman en la misma placa. Incluye la espera al
ciclo del guante, el ADC, el envío, la tarea de control y la espera al PWM; no incluye el movimiento
mecánico ni el camino de la muñeca.

**33. ¿De dónde salen los ~24 ms si cada etapa es tan rápida?**
Sobre todo de dos esperas que, en promedio, valen medio período cada una: hasta el siguiente ciclo del
guante (0–20 ms) y hasta el siguiente pulso PWM del servo (0–20 ms). Lo demás (ADC ~5 ms, ESP-NOW unos
pocos ms, la tarea < 1 ms) suma poco.

**34. ¿Por qué 50 Hz? ¿No sería mejor más rápido?**
Los servos reciben un pulso cada 20 ms (PWM de 50 Hz): mandar órdenes más seguido no las aplica antes.
Además los movimientos voluntarios de la mano son de pocos hertz (conocimiento general), así que 50 Hz
sobra. Subir a 100 Hz es trabajo futuro, pero exige escalar el limitador de velocidad, que avanza una
cantidad fija por ciclo.

**35. ¿Qué pasa si el receptor se reinicia en plena operación?**
Al arrancar, `setup()` acopla los servos directamente en la posición de reposo, así que la mano va a Home
a la velocidad propia de los servos (sin limitador) y queda esperando paquetes. Cuando llega el primero,
retoma desde ahí con el limitador.

**36. ¿Qué pasa al calibrar los dedos en plena operación?**
Desde la v2.3.0, mientras se calibra (comando `c`), el guante deja de leer los dedos, de transmitir y de
imprimir. A los 250 ms el receptor entra en failsafe y la mano va sola a Home; al terminar la calibración
vuelve a seguir al guante. El MPU se sigue leyendo para que el filtro no pierda el ángulo.

**37. ¿Dónde se guarda la calibración y qué pasa si se corta la luz?**
En la NVS (memoria no volátil de la flash) con la librería `Preferences`, clave por dedo. Sobrevive a un
apagado: al encender se carga sola. Hay una sola calibración guardada; guardar una por operador es
trabajo futuro.

---

## E. Preguntas incómodas (limitaciones conocidas)

Responder sin defenderse: reconocer, explicar el impacto y decir cómo se arregla.

**38. ¿Cualquiera con un ESP32 podría mover la mano?**
En principio sí, si conoce el formato del paquete: el receptor no filtra por MAC de origen ni usa
cifrado. El checksum protege contra errores y paquetes ajenos "por casualidad", no contra alguien que lo
haga a propósito. Se arregla comparando la MAC de origen en el callback (pocas líneas) o activando el
cifrado de ESP-NOW. Para una demo de laboratorio no fue prioridad; para un uso real sería obligatorio.

**39. ¿Usan el número de secuencia para algo?**
Hoy sólo se imprime en la telemetría y sirvió para alinear las curvas del guante y la mano en MATLAB. El
receptor no cuenta paquetes perdidos con él; hacerlo es la mejora M7 del backlog.

**40. En el callback usan `xQueueOverwriteFromISR`, ¿está bien?**
Funciona, pero no es la API indicada: el callback de ESP-NOW corre dentro de la tarea Wi-Fi, no en una
interrupción, así que lo correcto es `xQueueOverwrite`. La corrección es de una línea.

**41. ¿Qué pasa si el MPU6050 se desconecta durante el uso?**
No se detecta: el mensaje de arranque dice "reintentando", pero el código no reintenta, y tampoco hay
supervisión en operación. Es la mejora M5 del backlog. Los dedos seguirían funcionando.

**42. ¿Midieron la deriva de la muñeca a largo plazo o el alcance del enlace?**
No. La deriva en reposo, la repetibilidad al volver a una postura y el alcance quedaron como trabajo
futuro. Lo que sí está verificado en hardware: seguimiento de dedos y muñeca, failsafe al apagar el
guante, pausa al calibrar, latencia media y consumo.

**43. ¿Por qué dos corridas de latencia dan medias distintas (23,9 ms y 31,4 ms)?**
Son corridas distintas, una larga en bloques y otra corta de 28 ensayos individuales; las dos caen en el
rango esperado de uno a dos ciclos (la corta, entre 22,8 y 42,3 ms, sin *timeouts*). Con la diferencia de
fase entre el ciclo del guante y el PWM, la media de pocas muestras varía bastante. Para el objetivo del
proyecto alcanzaba con el orden de magnitud; la estadística completa es trabajo futuro.

**44. ¿La mano tiene realimentación? ¿Sabe si agarró algo?**
No. Los servos tienen su propio lazo cerrado de posición interno, pero el sistema no le devuelve nada al
operador: no hay sensores de fuerza ni realimentación háptica. Está en trabajo futuro.

---

## F. Conocimiento complementario

**45. ¿Cómo funciona un servo de hobby por dentro?**
Motor de continua + caja reductora + potenciómetro en el eje de salida + un circuito de control. El
circuito compara el ancho del pulso recibido con la posición que lee el potenciómetro y mueve el motor
hasta que coinciden: es un **lazo cerrado de posición** dentro del servo.

**46. ¿Cómo es la señal PWM de un servo?**
Un pulso cada 20 ms (50 Hz). Lo que importa es el **ancho** del pulso, no el ciclo de trabajo. Lo típico
es 1–2 ms; en el proyecto usamos 500–2400 µs para 0–180°, unos 10,6 µs por grado. En el ESP32 la genera
el periférico LEDC por hardware, sin ocupar la CPU.

**47. ¿Tiene relación la banda muerta del servo con la nuestra?**
Sí. La hoja del SG90 indica una banda muerta de unos 10 µs, del orden de 1° con nuestra escala. Órdenes
que cambian menos de eso el servo no las puede resolver bien y termina "cazando" la posición y zumbando;
por eso nuestra banda muerta de software es de 1° en los dedos.

**48. ¿Cómo mide un giróscopo MEMS?**
Una masa microscópica vibra dentro del chip; al girar, la fuerza de Coriolis la desplaza
perpendicularmente y eso se mide como un cambio de capacidad. Por eso mide **velocidad angular**, no
ángulo: para tener el ángulo hay que integrar, y cualquier *bias* se acumula (θ_error = b·t).

**49. ¿Y un acelerómetro MEMS?**
Una masa sostenida por resortes microscópicos que se desplaza con la aceleración, medida también por
capacidad. En reposo mide sólo la gravedad, y de sus componentes en X, Y, Z salen el *pitch* y el *roll*.

**50. ¿Qué diferencia hay entre el filtro complementario y el de Kalman?**
Los dos fusionan giróscopo y acelerómetro. El complementario usa un peso fijo (α). El Kalman calcula en
cada ciclo un peso óptimo según las incertidumbres del modelo y la medición, y además puede estimar el
*bias* del giróscopo; a cambio tiene tres parámetros (Q_angle, Q_bias, R) y más cuentas. Para dos ángulos
a 50 Hz, en la práctica rinden parecido, y el complementario es más fácil de ajustar y verificar.

**51. ¿Cómo funciona el bus I²C?**
Dos líneas compartidas: SDA (datos) y SCL (reloj), de colector abierto con resistencias de *pull-up*. El
maestro (ESP32) genera el reloj y direcciona a cada esclavo por una dirección de 7 bits (el MPU6050 es
0x68, o 0x69 si se pone en alto el pin AD0). El guante usa modo rápido (400 kHz).

**52. ¿Por qué se puede "colgar" el bus I²C y cómo se destraba?**
Si el ESP32 se resetea en medio de una lectura, el esclavo puede quedar sosteniendo SDA en bajo esperando
más pulsos de reloj. Mandando hasta 9 pulsos en SCL termina de sacar su byte y suelta la línea; después se
genera una condición de STOP. Es una técnica estándar y el guante la hace en cada arranque.

**53. ¿Cómo es el ADC del ESP32 y qué limitaciones tiene?**
Es de aproximaciones sucesivas (SAR), 12 bits (0–4095). Es ruidoso y no es lineal, sobre todo cerca de
los extremos del rango con atenuación de 11 dB (según la documentación de Espressif). Lo compensamos con
sobremuestreo (16 lecturas: el ruido baja a σ/4) y con la calibración de dos puntos, que usa los extremos
reales de cada dedo.

**54. ¿Qué es ESP-NOW a nivel de radio?**
Usa tramas de gestión de Wi-Fi (IEEE 802.11, de tipo *action* específicas del fabricante) en la banda de
2,4 GHz, sin asociarse a un router. Por eso no hay conexión que establecer y la latencia es baja. Admite
hasta 250 bytes por paquete (en la versión que usamos), ACK automático y cifrado opcional.

**55. ¿Por qué Fletcher-16 y qué limitación tiene?**
Detecta cualquier error de un bit y, a diferencia de una suma simple, depende de la posición de cada byte
(detecta bytes intercambiados). Es sólo sumas, sin tablas. Su limitación conocida: como trabaja módulo
255, no distingue un byte 0x00 de uno 0xFF. Un CRC detecta más patrones de error, pero la capa MAC de
Wi-Fi ya trae CRC-32, así que el nuestro es una segunda barrera.

**56. ¿Qué es un pin de *strapping*?**
Un pin cuyo nivel se lee en el instante del reinicio para configurar el chip (modo de arranque, tensión
de la flash, etc.). En el ESP32 son GPIO 0, 2, 5, 12 y 15. Conectarles cargas puede afectar el arranque
o el comportamiento, por eso se evitan para periféricos cuando hay alternativas.

**57. ¿Qué ejemplos reales de telerrobótica conocen?**
(Conocimiento general.) Manipuladores maestro-esclavo de las celdas calientes nucleares, robots para
desactivar explosivos, la cirugía robótica asistida (por ejemplo el sistema da Vinci, en el que el
cirujano opera desde una consola) y los brazos robóticos de exploración espacial. Todos comparten la idea
del proyecto: el operador lejos del riesgo y la máquina replicando sus movimientos.

**58. ¿Qué es la teleoperación bilateral y por qué es más difícil?**
Es cuando además de mandar posición al robot, se devuelve fuerza al operador (háptica). La latencia ahí
importa mucho más: un retardo en un lazo que se cierra a través del operador puede volverlo inestable.
En nuestro sistema, unilateral, la latencia sólo hace que la mano "llegue tarde", no la desestabiliza.

---

## G. Durante la demo

**59. Si algo falla en vivo, ¿qué miro primero?**

| Síntoma | Causa probable | Qué hacer |
|---|---|---|
| Telemetría del guante con `NO_ACK` permanente | La mano está apagada o cambió la placa (MAC) | Verificar alimentación del receptor; `macReceptor[]` |
| La mano vuelve sola a reposo | Failsafe: no llegan paquetes por más de 250 ms | Revisar que el guante esté encendido y cerca |
| Un dedo no abre o no cierra del todo | Calibración de otra persona o corrida | Mandar `c` por el monitor serie y recalibrar |
| La muñeca no queda centrada | Se encendió el guante moviendo la mano (*bias* mal tomado) | Reiniciar el guante con la mano quieta |
| Movimientos bruscos o reinicio de la mano | Alimentación de los servos | Revisar la fuente de 12 V y los cables |

**60. ¿Qué hacen si la demo directamente no funciona?**
Mostrar el video de respaldo (diapositiva 4) y explicar con la telemetría o el diagrama qué tendría que
pasar. No intentar depurar más de un minuto frente al jurado.
