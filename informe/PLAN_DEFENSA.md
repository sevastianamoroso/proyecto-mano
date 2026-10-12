# Plan de la presentación para la defensa (borrador)

Propuesta de estructura para armar después las diapositivas (Beamer) a partir del informe.
Duración, formato y tiempos a confirmar con la cátedra.

## Estructura propuesta (~15 diapositivas)

| # | Diapositiva | Contenido | Fuente en el informe |
|---|---|---|---|
| 1 | Portada | Título, autores, tutores | Portada |
| 2 | Motivación | Manipulación remota en entornos peligrosos | Sección 1 |
| 3 | Objetivo | Movimientos que se replican (5 dedos, rotación, flexión de muñeca) | Sección 2 |
| 4 | Demostración (video) | Guante y mano funcionando. **Grabarlo antes, por si falla el hardware en vivo** | — |
| 5 | Arquitectura | Diagrama en bloques (figura 3) | Sección 3.1 |
| 6 | Guante | Diseño 3D + sensores (potenciómetros, MPU6050) | Sección 4.1–4.3 |
| 7 | Mano robótica | Diseño 3D + servos | Sección 5.1 |
| 8 | Protocolo | Trama de 19 bytes, checksum Fletcher-16 | Sección 3.3 |
| 9 | Dedos | Sobremuestreo y calibración de dos puntos | Sección 4.4.3 |
| 10 | Muñeca | Filtro complementario: idea, ecuación, τ ≈ 1 s | Sección 4.4.4 |
| 11 | Receptor en tiempo real | Callback → cola → tarea; máquina de estados y failsafe | Sección 5.3 |
| 12 | Suavizado | Limitador de velocidad + banda muerta | Sección 5.3.3 |
| 13 | Evolución | Kalman → complementario y por qué; problemas encontrados en las pruebas | Sección 6 |
| 14 | Resultados | Latencia, pruebas con la versión actual | Secciones 7–8 |
| 15 | Conclusiones y trabajo futuro | | Sección 9 |

## Mensajes clave

1. El guante calcula todo; la mano sólo valida, suaviza y mueve (separación de responsabilidades).
2. El diseño se validó de forma iterativa en hardware (6 versiones, cada cambio con un motivo observado).
3. La seguridad está en el firmware: checksum, failsafe a 250 ms, límites mecánicos, rampa de velocidad.

## Preguntas probables del jurado (preparar respuesta)

- ¿Por qué filtro complementario y no Kalman? (sección 6.3)
- ¿Cuál es la latencia total y cómo la midieron? (sección 7: hoy es una **estimación**; conviene medirla antes de la defensa)
- ¿Qué pasa si se corta la comunicación? (failsafe, sección 5.3.2)
- ¿Alcanza la fuente para 7 servos? ¿Qué pasa con los picos de corriente? (sección 5.2: completar con lo implementado y medido)
- ¿Por qué ESP-NOW y no Wi-Fi con router o Bluetooth?
- ¿Por qué no se mide el *yaw*?
- ¿Cómo se adapta a otro operador? (recalibración en el guante)

## Para hacer antes de armar las diapositivas

- Cerrar las pruebas con la versión actual (figuras de la sección 8).
- Medir la latencia de punta a punta.
- Grabar el video de demostración.
