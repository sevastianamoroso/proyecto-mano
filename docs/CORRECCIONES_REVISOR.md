# Correcciones del revisor (Edu), recibidas 2026-10-27

> Notas para analizar **antes** de cambiar el informe. Todavía no se tocó nada de esto.

Mensaje original (resumen): el informe está muy bien, solo dos cosas menores:
1. En la pág. 14 del PDF, la ecuación (7) se describe como "filtro complementario" que se comporta como
   pasa-bajos para una medición y pasa-altos para la otra. Según él no hay pasa-altos en ningún canal.
2. La forma de medir la latencia no le quedó clara (dijo que la iba a releer).

---

## 1. Filtro complementario (`informe/capitulos/04_transmisor.tex:225-233`)

**Lo que dice hoy el informe** (después de `eq:complementario`): "El término del giróscopo actúa como un filtro
pasa-altos y el del acelerómetro como un pasa-bajos; sus transferencias suman uno en todas las frecuencias".

**El revisor tiene razón con la ecuación tal como está escrita.** Con $\alpha = 0{,}98$ y $\Delta t = 20$ ms,
tomando como entradas lo que aparece en la ecuación ($\theta_{acc}$ y $\omega_k\Delta t$):

$$
\Theta(z) = \frac{1-\alpha}{1-\alpha z^{-1}}\,\Theta_{acc}(z) + \frac{\alpha}{1-\alpha z^{-1}}\,\Delta t\,\Omega(z)
$$

- Los dos canales son **pasa-bajos** con el mismo polo en $z = \alpha$. No hay pasa-altos.
- Ganancia en continua: 1 para $\theta_{acc}$ y $\alpha/(1-\alpha) = 49$ para $\omega\Delta t$ (lo mismo que calcula él).
- Las transferencias **no** suman 1. Lo que suma 1 son los pesos, $(1-\alpha) + \alpha$.
  Su frase "sólo da 1 la suma de las ganancias en continua" parece referirse a esto (1 + 49 no da 1).

**De dónde sale el nombre "complementario".** Si la entrada del giróscopo se toma como el ángulo integrado
$\theta_{g,k} = \theta_{g,k-1} + \omega_k\Delta t$, es decir $\Delta t\,\Omega = (1-z^{-1})\Theta_g$:

$$
H_g(z) = \frac{\alpha(1-z^{-1})}{1-\alpha z^{-1}} \;\text{(pasa-altos: 0 en continua, ≈1 en Nyquist)},
\qquad H_{acc}(z) + H_g(z) = 1 \;\;\forall z
$$

Así, lo que dice el informe es correcto **solo con esta interpretación**, que el texto no explica: pasa de una
ecuación en $\omega_k\Delta t$ a hablar de un pasa-altos sin el paso intermedio.

**Dato extra que se desprende de la ganancia de 49.** Un *bias* residual $b$ del giróscopo no produce una deriva
sin límite, sino un error **fijo** de $\frac{\alpha}{1-\alpha}\,b\,\Delta t = \tau\,b \approx 0{,}98\,\mathrm{s}\cdot b$
(por ejemplo, 1 °/s de bias da ≈ 0,98° de error). Esto justifica la calibración del bias en reposo.

**Propuesta de corrección:**
- [ ] Reescribir el párrafo con las dos transferencias: la "directa" (coincide con el esquema del revisor) y la
      equivalente con $\theta_g$ (de donde sale el nombre).
- [ ] Sacar "el término del giróscopo actúa como pasa-altos" o aclarar que vale respecto del ángulo integrado.
- [ ] Opcional: agregar el diagrama de bloques del revisor y la cuenta del error por bias.
- [ ] **La presentación repite el error:** diapositiva "Muñeca: filtro complementario"
      (`informe/presentacion/presentacion.tex`), con el `\underbrace` "giróscopo: pasa-altos".
- [ ] Revisar si `capitulos/C_filtro_kalman.tex` dice algo parecido (no se verificó).
- [ ] Verificar que la "ecuación (7)" de la pág. 14 sea `eq:complementario` (se supuso, no se comprobó).

---

## 2. Medición de latencia (`informe/capitulos/07_analisis_temporal.tex`)

Lo que se encuentra poco claro al leerla:

| # | Problema | Qué tenemos en el repo y no está en el informe |
|---|---|---|
| 1 | Todo el montaje está en un solo párrafo, sin figura. "El receptor reemplaza al potenciómetro" se puede leer como que el receptor es a la vez el sistema medido y el instrumento. | Diagrama y tabla de conexiones en `docs/pruebas/PRUEBA_LATENCIA.md` §1 y §4 (Rx GPIO18 → 1 kΩ → Tx GPIO34 con el pote desconectado, puente GPIO26 → GPIO4, GND común). |
| 2 | No explica por qué medir con el mismo receptor no altera el resultado. | Los instantes los toman la ISR y el callback; `loop()` tiene prioridad 1; el Tx corre su firmware sin cambios. |
| 3 | Faltan criterios clave: por qué el flanco de bajada, qué cuenta como "ancho nuevo", cómo se elige el instante del escalón. | Umbral de 30 µs (≈ 3°), espera aleatoria de 200–600 ms (el escalón cae en cualquier punto del ciclo de 20 ms), un escalón por vez. |
| 4 | Dos corridas con resultados distintos (23,9 y 31,4 ms) presentadas juntas. La de ≈ 420 ensayos solo tiene medias de bloque, sin dispersión individual. La explicación por la fase entre placas se afirma pero no se demuestra (`NOTAS.md` dice que no consta cuándo se apretó EN). | `docs/pruebas/evidencia/2026-10-26_latencia/NOTAS.md` |
| 5 | No se informa cómo se reparte el total, aunque el dato existe. | `llegada_us` promedia 15,7 ms en la corrida de 28 (Tx + enlace frente a Rx + PWM). También `final_us`, que incluye el limitador de velocidad. |
| 6 | La tabla 7 sigue siendo solo una estimación. No se aplicaron las sugerencias del propio procedimiento. | `PRUEBA_LATENCIA.md` §9: tabla estimación frente a medición, histograma, diagrama de cajas por tramos. |

**Propuesta:**
- [ ] Reorganizar la sección en: *qué se mide* (línea de tiempo t0 → llegada → t1), *montaje* (figura y tabla de
      conexiones) y *resultados* (una tabla con n, media, desvío, mín. y máx. por corrida, más el reparto en tramos).
- [ ] Cerrar con las limitaciones de `PRUEBA_LATENCIA.md` (sin sensor físico, sin muñeca, cables entre placas).
- [ ] Opcional: repetir la corrida con registro por ensayo y reinicios con EN anotados, para tener una sola cifra
      sólida. No es imprescindible: el material actual alcanza si se reorganiza.

---

## Prioridad
1. **Filtro complementario:** corregir sí o sí (error conceptual concreto, un párrafo, en el informe y en la presentación).
2. **Latencia:** reorganizar el material que ya está; no hace falta volver a medir.
