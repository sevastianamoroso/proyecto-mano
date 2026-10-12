# Informe final (LaTeX / Overleaf)

Proyecto LaTeX del informe, basado en la plantilla MSNIE – DIEC – UNS. Documenta el firmware **v2.3.0**.

## Cómo subirlo a Overleaf

1. Comprimir **el contenido** de esta carpeta (`main.tex`, `capitulos/`, `Imagenes/`, `bib/`, `bst/`) en un `.zip`.
   La carpeta `referencia/` no hace falta subirla.
2. En Overleaf: **New Project → Upload Project** y elegir el `.zip`.
3. Compilador: **pdfLaTeX** (el de defecto). También compila con XeLaTeX.

Si se edita en Overleaf, conviene volver a descargar el proyecto y copiarlo acá para que el repositorio
quede actualizado (o editar acá y volver a subir).

## Estructura

| Archivo | Contenido |
|---|---|
| `main.tex` | Preámbulo, portada, orden de los capítulos y bibliografía |
| `capitulos/01_motivacion.tex` | Motivación |
| `capitulos/02_solucion.tex` | Solución propuesta y componentes |
| `capitulos/03_arquitectura.tex` | Diagrama en bloques, tareas FreeRTOS, protocolo y checksum |
| `capitulos/04_transmisor.tex` | Guante: mecánica, conexionado, IMU, firmware (dedos, filtro complementario, mapeo) |
| `capitulos/05_receptor.tex` | Mano: mecánica, alimentación, firmware (cola, máquina de estados, slew-rate, banda muerta) |
| `capitulos/06_evolucion.tex` | Evolución preliminar → v1.0.0 (Kalman) → v2.3.0 |
| `capitulos/07_analisis_temporal.tex` | Estimación de latencia |
| `capitulos/08_pruebas.tex` | Pruebas (todavía con las mediciones del firmware preliminar) |
| `capitulos/09_conclusiones.tex` | Conclusiones (a redactar) y trabajo futuro |
| `capitulos/A_ajuste_parametros.tex` | Apéndice: guía de ajuste de parámetros |
| `bib/IEEEreferencias.bib` | Referencias (estilo IEEE) |
| `referencia/` | `main.tex` original y PDF del informe preliminar, sólo como referencia |

## Pendientes

Todo lo que falta completar o confirmar está marcado en el PDF en **rojo** con `\pendiente{...}`.
Para listarlos: buscar `\pendiente` en la carpeta `capitulos/`.
