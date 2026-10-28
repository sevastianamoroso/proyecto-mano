# Guion de la presentación: para practicar

- Sigue el orden de `presentacion.tex` (versión con las diapositivas nuevas del MPU6050).
- **[entre corchetes]** van las acciones: qué señalar, cuándo pausar, a qué mirar.
- El texto es orientativo: conviene decirlo con palabras propias y no leerlo.
- El tiempo de cada diapositiva es estimado. En total da unos **22–25 min** sin la demo, y se puede exponer hasta 30–40 min, así que **no hace falta recortar**: conviene ir sin apuro y, si sobra tiempo, usar las diapositivas de respaldo. "Qué recortar", al final, queda sólo por si el jurado pide acortar.
- **Reparto:** completar quién presenta cada bloque. Conviene cambiar de orador al empezar una sección, no en medio de una diapositiva.

| Bloque | Diapositivas | Orador |
|---|---|---|
| Introducción | 1–4 | |
| Hardware | 5–9 | |
| Firmware | 10–21 | |
| Resultados y cierre | 22–27 | |

---

## Introducción

### 1. Portada (~30 s)
Buenos días. Somos Joaquín Petruf y Sebastián Amoroso, y les vamos a presentar nuestro proyecto final de Telerobótica: *una mano robótica telecomandada con sensado de movimientos*. Nuestros tutores fueron Rodrigo Santos y Gabriel Eggly.
**[Señalar la foto de la mano a la derecha]** Esta es la mano que construimos. En un rato la van a ver moverse.

### 2. Motivación (~1 min)
**[Recorrer el diagrama de izquierda a derecha]**
La idea parte de un problema concreto: hay tareas en las que la presencia de una persona es un riesgo inaceptable, como manipular sustancias tóxicas o material radiactivo, o trabajar con temperaturas extremas. **[Señalar los íconos dentro del recuadro rojo]**
La telerobótica resuelve eso separando al operador del lugar de trabajo. **[Señalar el guante]** El operador queda en una zona segura con un guante puesto, **[señalar las ondas]** el movimiento viaja por un enlace inalámbrico **[señalar la mano]** y una mano robótica lo replica en la zona de riesgo.
**[Señalar el recuadro "Idea"]** Esa es la propuesta: sensar la mano del operador y replicar sus movimientos de forma inalámbrica en una mano impresa en 3D.

### 3. Objetivo y componentes (~1 min)
Nos propusimos replicar siete movimientos: la apertura y el cierre de cada uno de los cinco dedos por separado, y dos de la muñeca, la rotación (pronosupinación) y la flexión–extensión.
**[Señalar la columna izquierda]** En el guante hay un ESP32, cinco potenciómetros (uno por dedo) y una IMU MPU6050 para la muñeca.
**[Señalar la columna derecha]** En la mano hay otro ESP32, cinco servos SG90 chicos para los dedos y dos MG946R, más robustos, para la muñeca, que mueve más carga.

### 4. Demostración (~2–3 min, según la demo)
**[Si la demo en vivo funciona]** Antes de entrar en detalle, les mostramos cómo funciona. **[Ponerse el guante]** Fíjense que puedo mover cada dedo por separado… **[cerrar el índice despacio y dejarlo a mitad de camino]** y la mano no sigue solo los extremos: también las posiciones intermedias. **[Girar la muñeca y después flexionarla]** La muñeca sigue la rotación y la flexión.
Si queda tiempo: **[apagar el guante]** si se corta el enlace, la mano vuelve sola al reposo en menos de un cuarto de segundo. Eso lo vamos a explicar en la parte de firmware.

---

## Hardware

### 5. Arquitectura del sistema (~1:30 min)
**[Recorrer el diagrama de izquierda a derecha]**
El sistema tiene dos nodos. **[Recuadro "Guante"]** Los potenciómetros y la IMU entran al ESP32 transmisor, que hace todo el cálculo: lee el ADC con sobremuestreo, fusiona la IMU con un filtro complementario, convierte todo a ángulos de servo y arma un paquete con checksum.
**[Flecha celeste]** El paquete viaja por ESP-NOW en 2,4 GHz: son 19 bytes, 50 veces por segundo.
**[Recuadro "Mano robótica"]** El receptor no calcula nada del operador. Valida el paquete, supervisa que el enlace siga vivo, suaviza el movimiento y genera el PWM de los siete servos.
**[Señalar el recuadro naranja de abajo]** Esta es la decisión de diseño principal: el guante calcula y la mano solo valida, suaviza y mueve. La ventaja práctica es que si cambia el operador se recalibra el guante y la mano no se toca.

### 6. Transmisor: el guante (~1:30 min)
**[Señalar la foto grande]** El guante lo diseñamos en Solid Edge y lo imprimimos en 3D.
**[Seguir las fotos chicas de izquierda a derecha mientras se nombran los pasos]**
Así llega el movimiento de un dedo al potenciómetro. Una tanza atada al dedal de la punta recorre el dorso del dedo, guiada por dedales en cada falange. **[Dedal]** En el dorso de la mano, la tanza tira de una palanca montada sobre el eje del potenciómetro. **[Palanca]** Sigue por un tubo con un resorte hasta la resortera. **[Resortera]** Cuando el dedo se abre, el resorte devuelve el eje al reposo y además mantiene la tanza tensa, que es lo que reduce la holgura.
**[Columna derecha, abajo]** El MPU6050 va paralelo al dorso de la mano. El ESP32 va en la muñeca, alimentado por USB desde la PC, que también usamos para la calibración y la telemetría.

### 7. Transmisor: conexionado (~45 s)
**[Señalar los potenciómetros en el diagrama]** El conexionado es simple: cada potenciómetro de 10 kΩ va entre 3,3 V y masa, y el cursor entra a un pin analógico.
Un detalle que importa: **[señalar los pines]** usamos solo el ADC1, porque en el ESP32 el ADC2 no funciona mientras la radio está activa.
**[Señalar el filtro RC a la derecha]** El filtro RC que se ve a la derecha es una mejora posible que dejamos planteada, pero no está implementada.

### 8. Receptor: la mano robótica (~1 min)
**[Señalar las dos fotos]** Esta es la mano. Los cinco SG90 van dentro de la palma, cada uno con una biela hacia su dedo.
La muñeca tiene dos MG946R: uno hace la rotación sobre un rodamiento de 20 mm y el otro la flexión, con una biela en el antebrazo.
**[Señalar el cuarto ítem]** Un criterio de diseño: el recorrido mecánico es mayor que el que permite el software, que va de 25° a 90°. Así el servo nunca llega a forzar el plástico contra un tope.
Todo se imprimió en una Hellbot Hidra.

### 9. Receptor: conexionado y alimentación (~1:15 min)
**[Señalar la fuente y el regulador en el diagrama]** Los servos se alimentan con una fuente de 12 V y 4 A, y un regulador buck MP1584 baja la tensión a 5 V.
**[Señalar el ESP32]** El ESP32 se alimenta aparte, por USB. ¿Por qué? Porque un pico de corriente de los motores podría bajar la tensión y reiniciarlo, lo que se llama *brownout*.
**[Columna derecha]** Para que las dos fuentes no se mezclen, quitamos el pin de 5 V del ESP32: el regulador no le inyecta corriente al USB y solo se unen las masas. Además pusimos capacitores de 470 µF y 100 nF a la entrada y a la salida del regulador. Más adelante van a ver que el de entrada redujo los picos de corriente a la mitad.

---

## Firmware

### 10. Firmware en tiempo real: dos núcleos (~1:30 min)
**[Señalar la tabla]** Los dos firmwares usan FreeRTOS y aprovechan los dos núcleos del ESP32.
En el núcleo 0 corre la pila de Wi-Fi y ESP-NOW, con sus callbacks, que hacen lo mínimo: validar y copiar. En el núcleo 1 corren nuestras tareas, fijadas con `xTaskCreatePinnedToCore`. Así la radio no le agrega *jitter* a nuestro ciclo de 20 ms.
**[Señalar el tercer ítem]** Entre núcleos: en el receptor pasamos los datos por una cola; en el transmisor alcanza con una bandera `volatile` para el ACK.
**[Señalar el recuadro de abajo]** Un detalle importante es el período. No usamos `delay(20)`, que espera 20 ms *después* del cómputo y hace que el período se estire. Usamos `vTaskDelayUntil`, que despierta en instantes absolutos: 0, 20, 40 ms. Si un ciclo se atrasa, el siguiente arranca enseguida y la grilla se mantiene.

### 11. Recorrido de un dato, del guante al servo (~1:30 min)
**[Seguir los números con el puntero, del 1 al 9]**
Sigamos un dato de punta a punta. En el transmisor, cada 20 ms: **(1)** se lee la IMU y se aplica el filtro, **(2)** se leen los dedos con 16 muestras cada uno, **(3)** se arma el paquete con el checksum y **(4)** se llama a `esp_now_send`. **(5)** La pila Wi-Fi copia el paquete en el núcleo 0 y lo transmite. **[Señalar la flecha punteada]** El ACK vuelve a `OnDataSent`, que solo actualiza la telemetría y no frena nada.
**[Pasar al lado del receptor]** **(6)** En el receptor, el callback valida el paquete y lo deja en una cola de un elemento. **(7)** Eso despierta a la tarea de control, que **(8)** aplica el failsafe, el limitador de velocidad y la banda muerta, y **(9)** escribe el PWM.
**[Señalar el primer ítem de abajo]** Hay dos cambios de núcleo, uno en cada placa. Y como la medición y el envío están en la misma tarea, en secuencia, nunca sale un paquete con una medición a medio hacer.

### 12. Protocolo de comunicación (~1:15 min)
**[Señalar el struct]** Este es el paquete completo: una firma `0xAA55`, un número de secuencia, los cinco ángulos de los dedos, los dos de la muñeca y un checksum. Son 19 bytes. El `pragma pack(1)` evita que el compilador agregue relleno, así la disposición es idéntica en las dos placas.
Usamos ESP-NOW porque no necesita router: va directo por MAC, en unicast y con ACK.
**[Columna derecha]** El receptor acepta un paquete solo si cumple tres condiciones: mide 19 bytes, empieza con la firma y el checksum coincide.
El checksum es Fletcher-16: **[señalar la fórmula]** dos sumas acumuladas módulo 255. Detecta errores de un bit e intercambios de bytes, y es más simple que un CRC. Sabemos que 802.11 ya tiene un CRC-32; el nuestro es una segunda barrera, por ejemplo ante otro dispositivo ESP-NOW cercano que mande algo que no es nuestro.

### 13. Sensado de los dedos (~1:15 min)
**[Bloque izquierdo]** Para los dedos atacamos dos problemas. Primero, el ruido del ADC: cada lectura es el promedio de 16 conversiones, y eso divide el desvío por raíz de 16, o sea por cuatro. Los cinco dedos se leen en unos 5 ms, dentro del ciclo de 20.
**[Bloque derecho]** Segundo, cada mano es distinta. Por eso hacemos una calibración de dos puntos: con la mano abierta y con el puño cerrado. Esos valores se guardan en la flash y, **[señalar la fórmula]** con ellos, la lectura se mapea linealmente al rango de 25° a 90°, con saturación.
**[Ítems de abajo]** La saturación protege la mecánica si el operador abre o cierra más que durante la calibración. Y desde la versión 2.3.0, mientras se calibra el guante no transmite, así la mano no imita los movimientos de la calibración.

### 14. IMU: cómo mide el MPU6050 (~1 min)
Pasamos a la muñeca. El MPU6050 tiene dos sensores MEMS.
**[Imagen izquierda]** El giróscopo tiene estructuras vibrantes. Cuando el sensor rota, la fuerza de Coriolis las deforma, y lo que mide es **velocidad angular**, no ángulo.
**[Imagen derecha]** El acelerómetro tiene una masa móvil entre peines fijos. La aceleración cambia la capacidad entre ellos, y lo que mide es **aceleración**, incluida la de la gravedad. Ese detalle es clave, como vamos a ver.
**[Tabla]** Lo configuramos en ±500 °/s y ±2 g. Elegimos 2 g porque una mano en uso normal no lo supera y es el rango con más resolución.

### 15. IMU: orientación a partir de la gravedad (~45 s) — *nueva*
**[Señalar la secuencia de bloques de arriba, de izquierda a derecha]** Acá se ve por qué sirve medir la gravedad. Con el sensor quieto, la única aceleración es la gravedad, que siempre apunta hacia abajo. Al inclinar el sensor, esa gravedad se reparte de otra forma entre los tres ejes. **[Señalar los ángulos θ y φ en los dibujos]** A partir de cuánto ve cada eje se obtiene la inclinación.
Las cuentas exactas, con `atan2`, las vemos en la próxima diapositiva.
> La figura está recortada: es sólo el esquema, sin las fórmulas con `atan` y θx/θy/θz del original (fig. 9 del informe), que no coinciden con las del firmware. Si preguntan por la fig. 9: es la misma idea; `atan2` resuelve el signo y el cuadrante.

### 16. IMU: qué aporta cada sensor y por qué deriva (~1:30 min)
**[Bloque izquierdo]** El giróscopo es rápido, pero para tener el ángulo hay que integrar la velocidad. Cualquier error constante, el *bias*, se acumula en el tiempo. **[Señalar el ejemplo]** Con un bias de 0,01 rad/s, que es poco más de medio grado por segundo, en un minuto el ángulo se corre unos 34°.
**[Bloque derecho]** El acelerómetro es lo opuesto: en reposo da la inclinación absoluta, sin deriva, con estas fórmulas. Pero cuando la mano acelera, mide la gravedad *más* esa aceleración, y el ángulo se llena de ruido.
**[Imagen de abajo]** De las tres rotaciones usamos dos: el *pitch*, para la flexión, y el *roll*, para la rotación. El *yaw* no se puede medir así: girar alrededor de la vertical no cambia lo que ve la gravedad, y para eso haría falta un magnetómetro.
**[Último ítem]** Al encender tomamos 500 lecturas en reposo para estimar el bias y restarlo.

### 17. IMU: deriva del giróscopo y ruido del acelerómetro (~1 min) — *nueva*
**[Señalar la curva verde]** Este gráfico muestra lo que acabamos de decir. La curva verde es el ángulo que da el giróscopo integrado. **[Seguir la flecha "DERIVA"]** Sigue bien los cambios, los escalones, pero se va corriendo hacia arriba y termina unos 50° por encima del valor real.
**[Señalar la curva azul]** La azul es el acelerómetro. Está centrada en el valor correcto, pero tiene un ruido de varios grados **[señalar las flechas "RUIDO"]** y picos cada vez que hay un movimiento brusco.
**[Señalar la curva roja]** La roja es la fusión de las dos, con el filtro complementario. Sigue los escalones tan rápido como el giróscopo y se queda centrada como el acelerómetro: sin deriva y sin ruido. Eso es lo que buscamos, y es lo que explicamos en la próxima diapositiva.
> Es una imagen ilustrativa, no una medición nuestra; la fuente (Luis Llamas) está citada al pie de la diapositiva. En el eje vertical, los valores por debajo de 0 no tienen el signo menos (viene así de origen): si preguntan, aclararlo.

### 18. Muñeca: filtro complementario (~1:30 min)
**[Señalar la ecuación]** El filtro es esta única línea. En cada ciclo, el ángulo nuevo se arma en un 98 % con lo que predice el giróscopo, que es el ángulo anterior más la velocidad por el Δt, y en un 2 % con el ángulo del acelerómetro.
¿Por qué se llama complementario? Si al canal del giróscopo lo pensamos en términos del **ángulo integrado**, su camino se comporta como un pasa-altos y el del acelerómetro como un pasa-bajos. Los dos se complementan, porque sus transferencias suman uno. **[Señalar τ]** La frontera entre ambos la da la constante de tiempo: τ ≈ 1 s.
**[Ítems]** En la práctica: los movimientos más rápidos que un segundo los sigue el giróscopo, sin retardo, y en tiempos más largos el acelerómetro corrige la deriva de a poco. Además, cerca de los 80° el roll queda indefinido (es el *gimbal lock*), así que retenemos el último valor. El filtro interno del MPU6050 lo pusimos en 44 Hz, que agrega unos 5 ms de retardo.
> La diapositiva ya está corregida: el pasa-altos se aclara respecto del ángulo integrado del giróscopo (ver la pregunta del revisor al final).

### 19. Receptor: callback, cola y tarea de control (~1:15 min)
**[Recorrer el diagrama]** Ahora el receptor. Llega el paquete por radio, el callback lo valida en el núcleo 0 y lo deja en una cola, y la tarea de control, en el núcleo 1, lo toma y escribe los servos.
**[Columna izquierda]** ¿Por qué una cola y no una variable global? Porque el callback y la tarea corren al mismo tiempo, en núcleos distintos. Con una global, la tarea podría leer un paquete copiado a medias: los dedos nuevos con la muñeca vieja. La cola copia los 19 bytes de forma protegida.
**[Columna derecha]** ¿Y por qué de un solo elemento? Porque solo importa el último paquete. Una cola larga acumularía paquetes viejos, o sea retardo. Con `xQueueOverwrite` el nuevo pisa al anterior y el callback nunca se bloquea. La tarea despierta apenas llega el paquete y, si no llega ninguno, igual despierta a los 20 ms para vigilar el enlace.

### 20. Receptor: máquina de estados y failsafe (~1 min)
**[Recorrer los estados siguiendo las flechas]** El receptor arranca en *esperando sincronización*, con los servos en reposo. Con el primer paquete válido pasa a operación normal y sigue al guante.
**[Flecha hacia abajo]** Si pasan 250 ms sin paquetes, entra en failsafe y lleva todos los objetivos a la posición de reposo. **[Flecha hacia la derecha]** Si se queda en reposo más de 2 s, apaga el PWM: los servos dejan de consumir y de calentarse.
**[Flechas de regreso]** Con un paquete válido vuelve a operación normal, partiendo de la última posición y sin saltos.
**[Primer ítem]** ¿Por qué 250 ms? Son unos 12 paquetes perdidos: tolera una interferencia breve y reacciona rápido ante un corte real.

### 21. Suavizado: limitador de velocidad y banda muerta (~1 min)
**[Señalar la ecuación]** En cada ciclo calculamos el error entre el objetivo y la posición actual. Si es mayor que la banda muerta δ, avanzamos, pero con un paso máximo. Si es menor, no movemos el servo.
**[Columna izquierda]** El limitador convierte un salto grande, como una reconexión o la salida del failsafe, en una rampa: menos picos de corriente y menos esfuerzo mecánico. La banda muerta evita que el servo corrija décimas de grado y zumbe.
**[Tabla]** Los límites están cerca de la velocidad real de los servos, así que en uso normal casi no agregan retardo: 8° por ciclo en los dedos, y 4° y 5° en la muñeca.

### 22. Evolución: cada cambio, un problema observado (~1:15 min)
**[Recorrer la tabla de arriba abajo, sin leerla entera]** Llegar hasta acá llevó seis versiones, y cada cambio respondió a un problema que vimos en el hardware.
La versión preliminar filtraba dos veces, en el guante y en la mano. **[Señalar el último ítem]** Los retardos se sumaban y los pasos cada vez más chicos caían en la zona muerta del servo: la mano zumbaba. En la 1.0 pasamos a FreeRTOS y agregamos un filtro de Kalman, pero la muñeca derivaba "como un mouse". En la 2.0 lo reemplazamos por el filtro complementario, más simple, y descubrimos que el problema real eran errores de signo y de inicialización. Las versiones siguientes bajaron la latencia y corrigieron detalles como la calibración.
**[Primeros ítems]** La 2.3.0 es la versión definitiva, probada en hardware. Cada versión es un *tag* de git, así que cualquiera se puede reproducir.

---

## Resultados

### 23. Latencia (~2 min)
> Esta diapositiva conviene explicarla despacio: el revisor dijo que el método de medición no le quedó claro.

**[Tabla izquierda]** Primero estimamos la latencia a partir del código y de las hojas de datos. Los dos términos que más pesan son las dos esperas: hasta el próximo ciclo del transmisor y hasta el próximo pulso PWM. Cada una va de 0 a 20 ms, con 10 de media. Sumando todo, la orden llega al servo unos 30 ms después del movimiento, en promedio. Después se suma el movimiento mecánico del servo, que es lo más lento: unos 100 ms cada 60°.
**[Columna derecha, despacio]** Después la medimos de punta a punta. El problema de medir entre dos placas es que cada una tiene su propio reloj, así que usamos uno solo, el del receptor. El receptor hace de instrumento:
1. **Estímulo:** desconectamos el potenciómetro del índice y, en su lugar, un pin del receptor aplica un escalón de tensión en la entrada del transmisor. El receptor anota ese instante.
2. **El transmisor no sabe que lo están midiendo:** corre su firmware normal, lee el escalón, lo envía, y el receptor lo procesa como cualquier paquete.
3. **Fin:** puenteamos la salida PWM del servo del índice a otro pin del receptor. Cuando termina el primer pulso con el ancho nuevo, el receptor anota el instante. Usamos el final del pulso porque es recién ahí cuando el servo conoce el ancho nuevo.
4. Los escalones se aplican en instantes aleatorios, así caen en cualquier punto del ciclo de 20 ms.

**[Señalar el resultado]** Sobre unos 420 escalones, la media fue de 24 ms, coherente con la estimación. Es bastante menos que lo que tarda el propio servo en moverse, así que el enlace no es lo que limita la respuesta.

### 24. Respuesta del sistema (v2.3.0) (~1:15 min)
**[Señalar los dos gráficos]** Para ver la respuesta registramos la telemetría de las dos placas a 50 Hz y alineamos los datos por número de secuencia. Arriba está la orden que manda el guante y abajo la posición que aplica la mano. A la izquierda, el pulgar; a la derecha, la rotación de la muñeca.
**[Recorrer las curvas]** La mano sigue la forma completa, incluidas las posiciones intermedias. **[Señalar una meseta]** En las mesetas, las variaciones de 1° no llegan al servo: es la banda muerta trabajando. **[Señalar un extremo]** En los extremos se ve la saturación entre 25° y 90°.
**[Texto de la derecha]** Una aclaración: el pequeño corrimiento entre curvas se debe al limitador y a la banda muerta, no a la latencia del enlace, que no se ve con este método. Para eso está la medición anterior.

### 25. Consumo del receptor (v2.3.0) (~1:15 min)
**[Imágenes de arriba]** Medimos la corriente con un shunt de 0,5 Ω en el negativo de la fuente de 12 V y el osciloscopio.
**[Señalar las dos primeras capturas]** En reposo se ven pulsos: es el *pulse-skipping* del MP1584, que es normal con poca carga. Comparen la captura sin capacitor de entrada con la que lo tiene: el pico baja de 0,38 a 0,18 A con la misma media.
**[Tercera captura y tabla]** Moviendo los dos ejes de la muñeca a la vez, la media es de 0,21 A con picos de hasta 0,72 A. El peor caso queda por debajo de 0,75 A, muy lejos de los 4 A de la fuente y de los 3 A del regulador. Y la salida de 5 V se mantiene entre 5,14 y 5,25 V.

### 26. Resumen de pruebas (v2.3.0) (~45 s)
**[Recorrer la tabla]** En resumen, con la versión final: el consumo está dentro de lo previsto, la tensión de los servos es estable, la calibración ya no se transmite a la mano, la respuesta sigue al guante en dedos y muñeca, la latencia media es de unos 24 ms y el failsafe lleva la mano al reposo si apagamos el guante.

---

## Cierre

### 27. Conclusiones (~1:30 min)
**[Ir ítem por ítem, sin leer]**
- Logramos un sistema de telerobótica de bajo costo que replica los cinco dedos y dos grados de libertad de la muñeca por ESP-NOW, incluidas las posiciones intermedias.
- La decisión que más nos simplificó el diseño fue separar responsabilidades: el guante calcula y la mano valida, suaviza y mueve.
- La seguridad está en el firmware: checksum, failsafe a los 250 ms, límites mecánicos y rampa de velocidad.
- El diseño fue iterativo y validado en hardware: seis versiones, cada una con un motivo observado.
- En tiempo real, la latencia media de unos 24 ms es menor que lo que tarda en moverse el propio servo.
- La alimentación quedó holgada y el ESP32 está aislado de los picos de los servos.
- **[Último ítem]** Y queremos ser claros con las limitaciones: no hay realimentación de fuerza, el *yaw* no se mide porque haría falta un magnetómetro y el sistema depende de una buena calibración.

### 28. ¡Gracias! (~10 s)
Muchas gracias. Quedamos a disposición para las preguntas.

---

## Respaldo (no se exponen: abrirlas solo si preguntan)

| Si preguntan por… | Ir a |
|---|---|
| Datos de la latencia, dispersión | *Respaldo: latencia medida (P11)* |
| Por qué no Kalman | *Respaldo: filtro de Kalman* |
| Cómo se convierte el ángulo de la IMU en ángulo de servo | *Respaldo: mapeo de la muñeca* |
| Más curvas de respuesta | *Respaldo: índice y muñeca vertical* |
| Corriente con el servo bloqueado, salida de 5 V | *Respaldo: servo bloqueado y salida de 5 V* |
| ESP-NOW, corte de enlace, fuente, yaw, otro operador, GPIO 23 | *Respaldo: preguntas probables* (ya tiene las respuestas) |

Más preguntas de práctica, con sus respuestas: `docs/aprendizaje/04_PREGUNTAS_DEFENSA.md`.

### Preguntas que conviene llevar preparadas (no están en el respaldo)

**"¿El filtro complementario es realmente un pasa-altos para el giróscopo?"** (punto marcado por el revisor)
> Con la ecuación tal como la escribimos, las dos entradas, θ_acc y ω·Δt, pasan por el mismo pasa-bajos con un polo en α. La ganancia en continua es 1 para el acelerómetro y α/(1−α) ≈ 49 para ω·Δt. El nombre "complementario" viene de tomar como entrada del giróscopo el ángulo integrado: ahí su transferencia es α(1−z⁻¹)/(1−αz⁻¹), un pasa-altos, y sumada a la del acelerómetro da exactamente 1. Una consecuencia práctica de esa ganancia de 49 es que un bias residual b del giróscopo no deriva sin límite: deja un error fijo de τ·b, del orden de 1° por cada °/s de bias.

**"¿Por qué en la latencia hay dos valores distintos (24 y 31 ms)?"**
> Hicimos dos corridas. En una se registraron unos 420 escalones promediados en bloques de 15 y dio 23,9 ms. En otra se registró cada escalón por separado, 28 ensayos, y dio 31,4 ms, con valores entre 22,8 y 42,3 ms y sin paquetes perdidos. Las dos placas trabajan a 50 Hz con relojes independientes que casi no se desfasan, así que la espera al próximo pulso PWM queda casi fija durante una corrida y cambia de una corrida a otra.

**"¿Medir con el mismo receptor no altera lo que se mide?"**
> Los instantes los toman una interrupción, que captura el flanco del PWM, y el callback de recepción. La lógica del ensayo corre en `loop()` con la menor prioridad, por debajo de la tarea de control. Y el transmisor corre su firmware normal, sin cambios.

---

## Qué recortar (sólo si piden acortar)
1. **Diapositiva 7 (conexionado del transmisor):** basta con una frase sobre ADC1/ADC2.
2. **Diapositiva 15 (orientación a partir de la gravedad):** se puede decir en una frase y pasar.
3. **Diapositiva 22 (evolución):** contar solo el doble filtrado y lo del Kalman.
4. **Diapositiva 26 (resumen de pruebas):** se puede saltear si ya se mostraron los resultados.
Con esto se ahorran unos 3–4 minutos. Para llegar a 15–20 min, además: resumir la 9 (alimentación) y la 10 (dos núcleos) en tres frases cada una, y en la 12 (protocolo) no explicar la fórmula del Fletcher-16.

## Consejos para el ensayo
- Practicar en voz alta con la presentación proyectada y un cronómetro, al menos dos veces completa.
- Señalar con el puntero lo que se nombra (por eso están las marcas **[…]**). Mirar al jurado, no a la pantalla.
- En las diapositivas con diagramas (5, 11, 19, 20) conviene recorrer el diagrama en el orden en que viaja el dato.
- Llevar el guante ya calibrado y probar la demo antes de entrar (no hay video de respaldo).
