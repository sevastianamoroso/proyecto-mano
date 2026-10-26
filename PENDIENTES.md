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
    se soltó el agarre del servo. No es una falla. Corregido en el informe el 2026-10-26.
  - La salida de 5 V del regulador se midió con carga y casi no varía (sin cifras registradas).
    Corregido en el informe el 2026-10-26.

## Estado del repositorio

Todo commiteado en `docs/presentacion`. **Por decidir:** llevarlo a `main` antes de la entrega.

## Hecho el 2026-10-22: anular en GPIO 23

- **Problema:** el dedo anular se movía con dificultad con el servo en el GPIO 12 (pin de *strapping*).
- **Prueba:** el mismo servo conectado al GPIO 13 (el del meñique) y al GPIO 23 respondió mucho mejor. La
  falla seguía al pin, no al servo ni a la mecánica.
- **Cambio:** `PIN_SERVO_ANULAR = 23`. Se eligió el 23 porque no es de *strapping*, no emite pulsos durante
  el arranque y no lo usa nada más. El GPIO 4 lo ocupa la medición de latencia en `Receptor_Latencia`.
- Ya se actualizaron el código, el informe, la presentación, el registro de pruebas y el diagrama.
- **Queda pendiente:**
  - [ ] Flashear `Receptor/Receptor.ino` con el 23 y confirmar con la mano completa.
  - Hecho el 2026-10-26: PNG del diagrama exportado de nuevo desde el `.drawio` (dice GPIO23) y revisado
    en el informe (fig. 17) y en la presentación (diapositiva 9).

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

- **Hecho el 2026-10-26 (P11):** media ≈ 24 ms (≈ 420 escalones en bloques de 15); otra corrida con 28 ensayos
  individuales: 22,8–42,3 ms. Cargado en el cap. 7 (con figura en pgfplots), el cap. 9, las diapositivas 21, 24, 25 y un
  respaldo, `EXPLICACION_CODIGO.md` y `REGISTRO_PRUEBAS.md`. Evidencia en `docs/pruebas/evidencia/2026-10-26_latencia/`.
  Decisión del usuario: alcanza con el valor medio; la caracterización exhaustiva queda como trabajo futuro.
- Observación sin resolver: el campo `final_us` (~310 ms) es mayor que lo que da el limitador según el código (~160 ms).
  No afecta a la latencia informada.

## 1b. Otras decisiones del 2026-10-26

- **Deriva y retorno de la muñeca (B1/B2/P13): no se hacen.** Decisión del usuario; quedó como trabajo futuro en el
  cap. 9 y se quitaron sus `\pendiente` (cap. 8 y 9) y la fila de la diapositiva de resumen.
- **5 V:** el "0,76 V pico a pico" era el parámetro `ampl` del LeCroy (no confiable a 1 V/div; su máximo fue 4,32 V).
  El cap. 8 ahora usa el RMS: 5,14–5,25 V en 57 adquisiciones, desvío 16 mV.

## 2. Comentario del profesor: figuras de señales poco legibles — hecho el 2026-10-26

- Sin datos crudos de las capturas: se engrosaron y oscurecieron las trazas con un script (sólo los píxeles de color de
  la zona del gráfico; grilla, texto y leyendas intactos) en las 10 del Serial Plotter (cap. 8.1) y las 5 del
  osciloscopio (P02/P03). En las P16 se recortó la leyenda con entradas sin usar. Los originales quedan en git (antes
  de este commit) y en `docs/pruebas/evidencia/2026-10-15/`.
- Las leyendas de las figuras del cap. 8.1 ahora dicen las unidades de los ejes.
- Sin resolver: los números de los ejes del Serial Plotter siguen chicos a ese ancho, y el ×10⁵ de las P16 sigue
  explicado sólo en el texto.

## 3. Comentario del profesor: resumir la evolución de versiones — hecho el 2026-10-26

- `06_evolucion.tex` pasó de 129 a 67 líneas (incluye las dos tablas, que se mantuvieron). Las ecuaciones,
  los parámetros y el código del Kalman están en el apéndice C (`C_filtro_kalman.tex`, `\label{sec:kalman}`).
- [ ] Que el usuario lo revise en el PDF.

## 4. Guía de estudio (`docs/aprendizaje/`)

- [ ] Que el usuario la lea.
- Hecho el 2026-10-26: los datos generales (tick, `loopTask`, ESP-NOW, `delay()`) se verificaron contra el
  core 2.0.17 y ESP-IDF 4.4.7; la tabla con las fuentes está al final de `docs/aprendizaje/README.md`.
- [ ] Agregar a la guía el caso del GPIO 12 → 23 como ejemplo de diagnóstico: intercambiar conectores para
  ver si la falla sigue al pin o al servo.
- [ ] Hallazgos de la revisión que podrían salir en la defensa (ya están en las preguntas de práctica):
  - el receptor no filtra por MAC de origen;
  - la secuencia no se usa para contar pérdidas;
  - en el callback, `xQueueOverwriteFromISR` debería ser `xQueueOverwrite`.

## 5. Pedidos del usuario (2026-10-25)

### Informe y presentación
- [x] **Latencia medida:** hecho el 2026-10-26 (ver sección 1).
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
    diapositiva de alimentación. Revisado el 2026-10-26: coincide con la hoja de datos (4,5–28 V, 3 A,
    100 kHz–1,5 MHz).
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
- Revisión del 2026-10-26 (compilada y vista página por página):
  - La diapositiva "Latencia" describía el método v1 (GPIO 2 → GPIO 4, sólo enlace y receptor); ahora
    describe el v2. En "Resumen de pruebas", P11 pasó a "Latencia de punta a punta".
  - Arreglos visuales: espacios en la ecuación del complementario, etiqueta tapada por una flecha en
    "Recorrido de un dato", cortes de palabra en la tabla de "Evolución", título largo de un respaldo.
  - [ ] Sigue pendiente el video de respaldo de la demo (diapositiva 4).

### Código
- [ ] **Comentarios:** reescribirlos para que suenen más naturales y humanos en `Transmisor.ino`,
  `Receptor.ino` y las variantes `_Latencia`. Por ejemplo, sacar los encabezados de tipo "Revisión de Grado
  en Ingeniería..." y las listas de "Mejoras clave". Si el informe cita fragmentos de código, actualizarlos
  también.

### Estudio
- [ ] **Preguntas:** generar más preguntas de estudio sobre la presentación y el informe, y agregarlas a
  `docs/aprendizaje/` (o a las de respaldo de la presentación).
