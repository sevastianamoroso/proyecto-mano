# Informe final (LaTeX / Overleaf)

Proyecto LaTeX del informe, basado en la plantilla MSNIE – DIEC – UNS. Documenta el firmware **v2.3.0**.

## Cómo compilarlo (local)

El informe tarda ~20 s en compilar, más que el límite del plan gratuito de Overleaf, así que se compila
localmente con [Tectonic](https://tectonic-typesetting.github.io), instalado en
`%LOCALAPPDATA%\Programs\tectonic\tectonic.exe`. La primera vez descarga los paquetes de LaTeX que
necesita (requiere internet); después funciona sin conexión.

- **VS Code** (recomendado): abrir la carpeta del repositorio, abrir `informe/main.tex` y guardar.
  La extensión *LaTeX Workshop* compila al guardar con la receta "Tectonic" (configurada en
  `.vscode/settings.json`) y muestra el PDF en una pestaña. Ctrl+clic en el PDF salta al código.
- **PowerShell**: `.\informe\compilar.ps1` → genera `informe/main.pdf`.

El PDF y los archivos intermedios están en `.gitignore`.

## Overleaf (opcional)

También compila en Overleaf (pdfLaTeX), pero en el plan gratuito puede superar el tiempo máximo.
Para subirlo: comprimir `main.tex`, `capitulos/`, `Imagenes/`, `bib/` y `bst/` y usar
**New Project → Upload Project**.

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
| `borradores/diagrama_firmware.tex` | Borrador: diagrama tipo clases del firmware (pendiente de revisar y anexar) |
| `bib/IEEEreferencias.bib` | Referencias (estilo IEEE) |
| `referencia/` | `main.tex` original y PDF del informe preliminar, sólo como referencia |

## Pendientes

Todo lo que falta completar o confirmar está marcado en el PDF en **rojo** con `\pendiente{...}`.
Para listarlos: buscar `\pendiente` en la carpeta `capitulos/`.
