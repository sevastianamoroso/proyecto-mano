# Pendientes (traspaso entre sesiones)

Actualizado: 2026-10-28. Archivo pensado para retomar desde otra PC u otra sesión de Claude. Borrar cada
ítem cuando se cierre y borrar el archivo cuando quede vacío.

## Contexto

- **Informe:** reentregado el 2026-10-26 como PDF (`informe/main.pdf`, compilado en local). Sin `\pendiente`.
- **Defensa: 2026-10-30.** Presentación en `informe/presentacion/presentacion.tex` (no se entrega). Expositores:
  Joaquín Petruf y Sebastián Amoroso; hasta 30–40 min + demo en vivo.
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

Hecho el 2026-10-28 (detalle en `docs/CORRECCIONES_REVISOR.md`): el cap. 4 explica las dos lecturas del filtro
complementario (pasa-bajos con la ecuación tal cual; pasa-altos respecto del ángulo integrado) y el error fijo τ·b
por *bias*; la diapositiva del filtro ya no dice "giróscopo: pasa-altos"; el cap. 7 se reorganizó (qué se mide,
montaje con figura, criterios, tablas por corrida y por tramos, limitaciones). La ecuación (7) de la pág. 14 es
`eq:complementario`; `C_filtro_kalman.tex` no tenía el error. `main.pdf` ya está recompilado.

- [ ] Que el usuario revise los cambios del informe y **reenvíe el PDF** a Edu.

### Presentación

- 2026-10-28: la diapositiva "Orientación a partir de la gravedad" usa `Acelerometro_Esquema.png` (la fig. 9
  recortada, sin las fórmulas con `atan` que no coinciden con el firmware). `Deriva_Ruido_MPU6050.jpeg` cita
  la fuente (Luis Llamas, luisllamas.es). Se puede exponer 30–40 min: no hace falta recortar.
- [ ] **Guion para practicar:** `informe/presentacion/GUION_PRESENTACION.md`. Reparto definido el 2026-10-28
  (ideas intercaladas, ≈ 14 min cada uno; orador en cada título). Falta ensayar en voz alta y repartir entre
  los dos el texto de la introducción (diapositivas 1–4). Dura ≈ 31 min sin demo.

### Otros

- [ ] Flashear `Receptor/Receptor.ino` (anular en GPIO 23) y confirmar con la mano completa. El usuario lo hace
  a último momento; el código y los documentos ya dicen GPIO 23.
- [ ] Revisar la presentación completa en el proyector o a pantalla completa.
- [ ] Guía de estudio (`docs/aprendizaje/`): que el usuario la lea, en especial `04_PREGUNTAS_DEFENSA.md`
  (60 preguntas, agregada el 2026-10-27, con el caso GPIO 12 → 23 y las limitaciones conocidas).

- Video de respaldo: descartado el 2026-10-28; la demo es en vivo (el prototipo anda bien). Se quitó de la
  diapositiva 4 y del guion.

## Opcional / después de la defensa

- [ ] Latencia: el campo `final_us` (~310 ms) es mayor que lo que da el limitador según el código (~160 ms). No
  afecta a la latencia informada. El script `docs/pruebas/matlab/latencia_punta_a_punta.m` nunca se probó.
- [ ] `REGISTRO_PRUEBAS.md`: P03 sigue en ⚠️; decidir si pasa a ✅. Además su fila dice que la salida de 5 V
  quedó "sin valores registrados", pero el informe da 5,14–5,25 V: alinearla.
- [ ] En GitHub siguen las ramas `docs/presentacion` (ya en `main`), `exp/latencia-v2` (2 commits fuera de
  `main`) y `exp/medicion-latencia` (8). Decidir si se borran; lo útil está en los tags.
