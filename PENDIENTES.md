# Pendientes (traspaso entre sesiones)

Actualizado: 2026-10-27. Archivo pensado para retomar desde otra PC u otra sesión de Claude. Borrar cada
ítem cuando se cierre y borrar el archivo cuando quede vacío.

## Contexto

- **Informe:** reentregado el 2026-10-26 como PDF (`informe/main.pdf`, compilado en local). Sin `\pendiente`.
- **Defensa: 2026-10-30.** Presentación en `informe/presentacion/presentacion.tex` (no se entrega). Expositores:
  Joaquín Petruf y Sebastián Amoroso; 15–20 min + demo en vivo.
- **Repositorio:** todo está en `main`. Las ramas viejas se borraron; lo que servía de ellas quedó en tags:
  `v1.0.0`…`v2.3.0` (versiones del firmware) y `pruebas-2026-10-15` (telemetría para MATLAB, graficador y
  `PLAN_LABORATORIO.md` de las pruebas P02, P03 y P16).
- **Decisiones del usuario (no reabrir):**
  - Latencia (P11): alcanza con la media (≈ 24 ms); la caracterización completa es trabajo futuro.
  - Deriva y retorno de la muñeca: no se hacen; quedó como trabajo futuro.
  - Corte de ~0,4 ms en P03 = instante en que se soltó el servo; no es una falla.
  - 5 V: se informa el valor eficaz (5,14–5,25 V, desvío 16 mV); el "0,76 V" era el parámetro `ampl` del
    osciloscopio, no confiable a 1 V/div.

## Antes de la defensa

### Corrección del revisor (Edu, 2026-10-27)

Análisis completo en `docs/CORRECCIONES_REVISOR.md`. **Todavía no se cambió nada del informe**: el usuario
quiere analizarlo antes.

- [ ] **Filtro complementario** (`informe/capitulos/04_transmisor.tex`, párrafo después de `eq:complementario`).
  Con la ecuación como está escrita no hay pasa-altos: las dos entradas pasan por el mismo pasa-bajos
  (ganancias en continua 1 y 49). El pasa-altos aparece sólo si la entrada del giróscopo es el ángulo
  integrado. Reescribir el párrafo con las dos transferencias.
- [ ] **El mismo error en la presentación:** diapositiva "Muñeca: filtro complementario", con el `underbrace`
  "giróscopo: pasa-altos". El guion ya lo explica bien, pero la diapositiva no.
- [ ] Verificar que la "ecuación (7) de la pág. 14" sea `eq:complementario` y revisar `C_filtro_kalman.tex`.
- [ ] **Latencia** (`informe/capitulos/07_analisis_temporal.tex`): reorganizar en qué se mide, montaje (con
  figura) y resultados (una tabla por corrida y reparto en tramos con `llegada_us`). No hace falta volver a
  medir. Detalle de los 6 puntos en `docs/CORRECCIONES_REVISOR.md`.
- [ ] Recompilar el informe y reenviar el PDF cuando estén las correcciones.

### Presentación (cambios del 2026-10-27)

- Se agregaron dos diapositivas en la sección del MPU6050: "Orientación a partir de la gravedad" (figura
  `Acelerometro_Calculo.png`, la fig. 9 del informe) y "Deriva del giróscopo y ruido del acelerómetro"
  (`informe/Imagenes/Deriva_Ruido_MPU6050.jpeg`).
- [ ] **Citar la fuente** de `Deriva_Ruido_MPU6050.jpeg`: no es una medición propia y no se sabe de dónde
  salió. Además, en el eje Y los valores negativos no tienen el signo menos (viene así de origen).
- [ ] La fig. 9 usa fórmulas con `atan` y nombres θx/θy/θz; la diapositiva siguiente usa `atan2` (las del
  firmware). Decidir si se aclara en la diapositiva o sólo de palabra (el guion lo menciona).
- [ ] **Guion para practicar:** `informe/presentacion/GUION_PRESENTACION.md`, con qué decir y qué señalar en
  cada diapositiva. Completar la tabla de quién presenta cada bloque. Dura unos 22–25 min sin demo y la
  defensa es de 15–20 min: **aplicar los recortes** de la sección "Qué recortar" y cronometrar.

### Otros

- [ ] Flashear `Receptor/Receptor.ino` (anular en GPIO 23) y confirmar con la mano completa. El usuario lo hace
  a último momento; el código y los documentos ya dicen GPIO 23.
- [ ] Video de respaldo de la demo (diapositiva 4 de la presentación).
- [ ] Revisar la presentación completa en el proyector o a pantalla completa.
- [ ] Guía de estudio (`docs/aprendizaje/`): que el usuario la lea, en especial `04_PREGUNTAS_DEFENSA.md`
  (60 preguntas, agregada el 2026-10-27, con el caso GPIO 12 → 23 y las limitaciones conocidas).

## Opcional / después de la defensa

- [ ] Latencia: el campo `final_us` (~310 ms) es mayor que lo que da el limitador según el código (~160 ms). No
  afecta a la latencia informada. El script `docs/pruebas/matlab/latencia_punta_a_punta.m` nunca se probó.
- [ ] `REGISTRO_PRUEBAS.md`: P03 sigue en ⚠️; decidir si pasa a ✅.
