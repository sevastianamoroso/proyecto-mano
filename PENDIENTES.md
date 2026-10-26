# Pendientes (traspaso entre sesiones)

Actualizado: 2026-10-26. Archivo pensado para retomar desde otra PC u otra sesión de Claude. Borrar cada
ítem cuando se cierre y borrar el archivo cuando quede vacío.

## Contexto que no está en el código

- **Defensa: 2026-10-30.** Presentación en Beamer en `informe/presentacion/presentacion.tex`. Expositores:
  Joaquín Petruf y Sebastián Amoroso; 15–20 min + demo en vivo.
- **Lo más actualizado está en `main`.** Las ramas `exp/*` pueden estar muy atrás: comparar con `main`
  antes de trabajar. `docs/presentacion` = `main` + la presentación.
- Aclaraciones ya confirmadas (no presentarlas como problemas abiertos):
  - El corte de ~0,4 ms al final de la captura "servo bloqueado" (P03, 2026-10-15) es el instante en que
    se soltó el agarre del servo. No es una falla. El informe todavía dice "sin explicar"; lo corrige el
    usuario.
  - La salida de 5 V del regulador se midió y casi no varía. El informe dice que no se midió la mínima en
    un pico; lo corrige el usuario.

## Estado del repositorio (sin commitear)

Rama actual: `docs/presentacion`. **Nada de lo siguiente está commiteado**; la carpeta se sincroniza por
OneDrive, no por git.

| Cambio | Archivos |
|---|---|
| Guía de estudio para la defensa (FreeRTOS, ESP-NOW, código parte por parte, preguntas) | `docs/aprendizaje/` + enlace en `README.md` |
| Firmware de medición de latencia v2 (2026-10-25): receptor nuevo, guía y script | `Receptor_Latencia/`, `docs/pruebas/PRUEBA_LATENCIA.md`, `docs/pruebas/matlab/latencia_punta_a_punta.m` |
| Método v1 archivado (firmware del usuario y captura del Serial Plotter) | `docs/pruebas/latencia_v1/` |
| Anular pasado de GPIO 12 a GPIO 23 (ver abajo) | `Receptor/Receptor.ino`, `Receptor_Latencia/Receptor_Latencia.ino`, `EXPLICACION_CODIGO.md`, `informe/capitulos/05_receptor.tex`, `informe/presentacion/presentacion.tex`, `Receptor/Conexionado_Receptor.drawio`, `informe/Imagenes/Conexionado_Receptor_Diagrama.png`, `docs/pruebas/REGISTRO_PRUEBAS.md` |

**Por decidir:** en qué rama se commitea cada cosa. El cambio de pin es firmware y convendría llevarlo a
`main` (por ejemplo en `fix/pin-anular`). La guía y la latencia, en sus propias ramas o donde indique el
usuario.

## Hecho el 2026-10-22: anular en GPIO 23

- **Problema:** el dedo anular se movía con dificultad con el servo en el GPIO 12 (pin de *strapping*).
- **Prueba:** el mismo servo conectado al GPIO 13 (el del meñique) y al GPIO 23 respondió mucho mejor. La
  falla seguía al pin, no al servo ni a la mecánica.
- **Cambio:** `PIN_SERVO_ANULAR = 23`. Se eligió el 23 porque no es de *strapping*, no emite pulsos durante
  el arranque y no lo usa nada más. El GPIO 4 lo ocupa la medición de latencia en `Receptor_Latencia`.
- Ya se actualizaron el código, el informe, la presentación, el registro de pruebas y el diagrama.
- **Queda pendiente:**
  - [ ] Flashear `Receptor/Receptor.ino` con el 23 y confirmar con la mano completa.
  - [ ] El PNG del diagrama (`informe/Imagenes/Conexionado_Receptor_Diagrama.png`) se corrigió editando la
    imagen; el original está en el scratchpad de la sesión. Lo prolijo es exportarlo de nuevo desde
    `Receptor/Conexionado_Receptor.drawio` (app.diagrams.net → Exportar → PNG), que ya dice GPIO23.
  - [ ] Recompilar el informe y la presentación y revisar las páginas afectadas.

## 1. Medición de latencia

**2026-10-25: método rehecho (v2).** El paso a paso está en `docs/pruebas/PRUEBA_LATENCIA.md`.
- **Estímulo y captura.** El receptor aplica un escalón en el GPIO 34 del TX, en lugar del potenciómetro del
  índice. Mide hasta el fin del primer pulso PWM cambiado del servo del índice, puenteando el GPIO 26 al
  GPIO 4.
- **Firmware.** El TX corre su firmware normal. El RX es `Receptor.ino` con agregados marcados `[LATENCIA]`;
  compila con el core 2.0.17.
- **Qué mide.** Es una latencia de punta a punta del camino de los dedos: incluye la espera al ciclo del TX
  y al período PWM, con los tramos separados. No incluye el movimiento mecánico ni el camino del IMU.
- **Método v1 archivado** en `docs/pruebas/latencia_v1/`, con sus problemas:
  - el emparejamiento era ambiguo;
  - medía solo el enlace y el receptor;
  - la captura tenía interpolación;
  - las estadísticas no se imprimían;
  - `VEL_MAX_MUNIECA_*` no coincidían con el firmware real.

- [ ] Hacer la prueba: unos 500 ensayos, con 2 o 3 reinicios del RX durante el registro.
- [ ] El script de MATLAB no fue probado: revisar que corra con el primer registro.
- [ ] Con los resultados, actualizar el capítulo 7, el capítulo 9 (su `\pendiente` de latencia), la
  presentación y `EXPLICACION_CODIGO.md` §6 y §9. Ver la sección 9 de la guía.
- [ ] Registrar la prueba en `docs/pruebas/REGISTRO_PRUEBAS.md`.

## 2. Comentario del profesor: figuras de señales poco legibles

| Figuras | Por qué cuesta leerlas | Arreglo propuesto |
|---|---|---|
| Serial Plotter: `Pitch_*`, `Roll_*`, `Pote_*`, `Transmicion_*` (cap. 8) | Líneas finas de color pastel sobre blanco, letra chica, *Interpolate* activado, a 0,45 del ancho de página | Lo ideal es regraficarlas en MATLAB desde los datos. Si no están, una figura por fila y oscurecer las líneas con un script |
| Osciloscopio: `Mediciones/P02_*`, `P03_*` | Traza amarilla sobre fondo blanco: casi no hay contraste | Recolorear el amarillo a azul oscuro con un script (sólo cambia el color) o exportar de nuevo desde el LeCroy |
| MATLAB: `Mediciones/P16_*` | Seis entradas grises de la leyenda sin usar; el factor ×10⁵ del eje quedó fuera de la captura | Exportar de nuevo mostrando sólo la señal graficada, con el eje en segundos |

- [ ] Falta preguntar al usuario: ¿hay datos crudos (CSV o texto del monitor serie) de las capturas del
  Serial Plotter?
- [ ] Guardar los originales antes de tocar cualquier imagen.

## 3. Comentario del profesor: resumir la evolución de versiones (`06_evolucion.tex`, ~130 líneas)

Propuesta, que dejaría el capítulo en unas 50 líneas:
- **Mantener** la tabla de versiones y la tabla de problemas de la versión preliminar.
- **Kalman:** un párrafo con la idea y las 4 causas de falla en una línea cada una. Las ecuaciones y el
  listado van a un apéndice o se eliminan.
- **v2.0 a v2.3:** dos o tres líneas cada una.

- Decidido el 2026-10-26: las ecuaciones y el código del Kalman van a un apéndice (C).
- [ ] Revisar las referencias cruzadas (`\ref`) que apuntan a las subsecciones que se eliminen.

## 4. Guía de estudio (`docs/aprendizaje/`)

- [ ] Que el usuario la lea. Antes de pasar a las diapositivas, verificar en la documentación de Espressif
  estos datos generales: tick de 1 ms, `loopTask` con prioridad 1 en el núcleo 1, ESP-NOW con 250 bytes y
  20 peers, y `delay()` = `vTaskDelay()`.
- [ ] Agregar a la guía el caso del GPIO 12 → 23 como ejemplo de diagnóstico: intercambiar conectores para
  ver si la falla sigue al pin o al servo.
- [ ] Hallazgos de la revisión que podrían salir en la defensa (ya están en las preguntas de práctica):
  - el receptor no filtra por MAC de origen;
  - la secuencia no se usa para contar pérdidas;
  - en el callback, `xQueueOverwriteFromISR` debería ser `xQueueOverwrite`.

## 5. Pedidos del usuario (2026-10-25)

### Informe y presentación
- [ ] **Latencia medida:** agregar al informe las mediciones nuevas, que cubren sólo el enlace y el
  receptor. Ver la sección 1 para el método, los problemas encontrados y qué filas del capítulo 7
  reemplazan.
- [ ] **GPIO 12 → 23:** cambiarlo en todos los lugares donde aparezca. Revisado el 2026-10-25: el código,
  el informe, la presentación y la documentación ya dicen GPIO 23. Las menciones al GPIO 12 que quedan
  cuentan la historia del cambio, y `informe/referencia/` es el informe original. Falta lo de la sección
  "Hecho el 2026-10-22": exportar de nuevo el PNG desde el `.drawio`, flashear y recompilar.
- [ ] **Gráficas poco visibles:** reemplazarlas por versiones más legibles. Ver la sección 2.
  - [ ] **Capturas nuevas del Serial Plotter, sin usar por ahora.** El usuario dejó 4 (pulgar, índice,
    muñeca rotacional y vertical), copiadas en `informe/Imagenes/Respuesta_*.jpeg`; las fuentes siguen en
    la raíz del repo. Se probaron en las figuras 25 y 26 y el 2026-10-25 se volvió a las de MATLAB
    (`Mediciones/P16_matlab_*.jpg`), porque **son otra medición**: otros movimientos y valores, 4 señales,
    sin alineación por secuencia y con *Interpolate* activado. El texto de la sección 8.2 (valores,
    saturación, escalonamiento, eje ×10⁵) describe las de MATLAB. Queda para después decidir dónde se usan
    y, si van en 8.2, reescribir ese texto (hace falta saber con qué firmware y a qué tasa se tomaron).
  - Faltan las otras figuras de la sección 2 (Serial Plotter de la versión preliminar, osciloscopio).
- [ ] **MP1584:** nombrarlo y mostrar al menos un parámetro en el informe y en la presentación.
  - El informe (`05_receptor.tex`) ya lo describe y tiene una tabla con sus parámetros.
  - Hecho en la presentación el 2026-10-25: rango de entrada, corriente y frecuencia de conmutación en la
    diapositiva de alimentación. Falta sólo revisarlo.
- [ ] **IMU:** explicar la deriva del giróscopo y qué aporta cada sensor a la medida:
  - el giróscopo responde rápido pero deriva al integrar;
  - el acelerómetro es absoluto (referencia: la gravedad) pero ruidoso ante el movimiento;
  - incluir valores (sensibilidad, rango, bias medido en `calibrarIMU`, α = 0,98).
  - Hecho en la presentación el 2026-10-25: diapositivas "IMU: cómo mide el MPU6050" e "IMU: qué aporta
    cada sensor y por qué deriva". El ejemplo de deriva usa b = 0,01 rad/s, el orden esperado según
    `PLAN_PRUEBAS.md` (A2), no un valor medido. Informe hecho el 2026-10-26 (`04_transmisor.tex`: sensibilidades y ecuación de deriva). El bias medido sigue sin registrarse: anotar el valor que imprime `[MPU6050] Bias gyro` al encender.
- [ ] **Colas en el receptor:** explicar la cola de un elemento con `Overwrite` y por qué no se usa una
  variable global (el callback está en el núcleo 0 y la tarea de control en el núcleo 1).
  - Hecho en la presentación el 2026-10-25 (diapositiva "Receptor: callback, cola y tarea de control").
    Informe hecho el 2026-10-26 (`05_receptor.tex`). El informe ahora nombra `xQueueOverwriteFromISR`, como el código.
- [ ] **`tareaTransmision` y núcleos 0/1:** explicar qué corre en cada núcleo y por qué, el recorrido de
  un dato del guante al servo, y qué pasa si un ciclo excede los 20 ms (`vTaskDelayUntil` no espera y la
  grilla se mantiene). Se habló en la sesión del 2026-10-24; la base está en
  `docs/aprendizaje/01_FREERTOS.md`.
  - Hecho en la presentación el 2026-10-25: "Firmware en tiempo real: dos núcleos" y la nueva "Recorrido
    de un dato, del guante al servo". Informe hecho el 2026-10-26 (`03_arquitectura.tex`: reparto entre núcleos y recorrido; `04_transmisor.tex`: ciclo que excede los 20 ms).

### Sólo presentación (`informe/presentacion/presentacion.tex`) — hecho el 2026-10-25
- [x] **Tema oscuro y estética:** fondo oscuro, portada con franja blanca para los logos, título con
  línea de acento y sección a la derecha, barra de avance en el pie, bloques TikZ recoloreados.
- [x] **Motivación:** esquema operador → enlace → mano en zona de riesgo, con íconos dibujados en TikZ.
- [x] **IMU:** figuras `Giroscopio.png`, `Acelerometro.png` y `Rotaciones.png`.
- [x] **Quitadas:** "¿Por qué complementario y no Kalman?" y "Trabajo futuro". "Respaldo: filtro de
  Kalman" se dejó en el apéndice (no se expone); borrarla si no se quiere.
- [x] **Corregido de paso:** la diapositiva de latencia decía GPIO23 → GPIO34 para la medición por cable;
  el código usa GPIO 2 (TX) → GPIO 4 (RX).
- [ ] Revisar la presentación completa en el proyector o a pantalla completa.

### Código
- [ ] **Comentarios:** reescribirlos para que suenen más naturales y humanos en `Transmisor.ino`,
  `Receptor.ino` y las variantes `_Latencia`. Por ejemplo, sacar los encabezados de tipo "Revisión de Grado
  en Ingeniería..." y las listas de "Mejoras clave". Si el informe cita fragmentos de código, actualizarlos
  también.

### Estudio
- [ ] **Preguntas:** generar más preguntas de estudio sobre la presentación y el informe, y agregarlas a
  `docs/aprendizaje/` (o a las de respaldo de la presentación).
