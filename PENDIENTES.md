# Pendientes (traspaso entre sesiones)

Actualizado: 2026-10-26. Archivo pensado para retomar desde otra PC u otra sesión de Claude. Borrar cada
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

- [ ] Flashear `Receptor/Receptor.ino` (anular en GPIO 23) y confirmar con la mano completa. El usuario lo hace
  a último momento; el código y los documentos ya dicen GPIO 23.
- [ ] Video de respaldo de la demo (diapositiva 4 de la presentación).
- [ ] Revisar la presentación completa en el proyector o a pantalla completa.
- [ ] Guía de estudio (`docs/aprendizaje/`): que el usuario la lea; agregar más preguntas y el caso del
  GPIO 12 → 23 como ejemplo de diagnóstico (intercambiar conectores para ver si la falla sigue al pin o al servo).
  Hallazgos que pueden salir en la defensa (ya están en las preguntas de práctica): el receptor no filtra por
  MAC de origen; la secuencia no se usa para contar pérdidas; en el callback, `xQueueOverwriteFromISR` debería
  ser `xQueueOverwrite`.

## Opcional / después de la defensa

- [ ] Latencia: el campo `final_us` (~310 ms) es mayor que lo que da el limitador según el código (~160 ms). No
  afecta a la latencia informada. El script `docs/pruebas/matlab/latencia_punta_a_punta.m` nunca se probó.
- [ ] `REGISTRO_PRUEBAS.md`: P03 sigue en ⚠️; decidir si pasa a ✅.
