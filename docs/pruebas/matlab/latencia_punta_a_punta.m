%% Latencia de punta a punta: escalón en la entrada del índice -> pulso PWM del servo del índice
% Requiere: Transmisor/Transmisor.ino (sin cambios) y Receptor_Latencia/Receptor_Latencia.ino,
% con las conexiones descritas en docs/pruebas/PRUEBA_LATENCIA.md.
% Líneas que interesan del receptor:
%   L,n,nivel,secuencia,llegada_us,primer_us,final_us,pulsos_perdidos
%   T,n,nivel,etapa
% El resto (telemetría de 5 Hz, arranque) se ignora.
%
% Requiere MATLAB R2020a o posterior, sin toolboxes. No fue probado en esta versión.

modo = "serial";          % "serial": registra del puerto | "archivo": analiza un .txt ya guardado
puertoRx = "COM6";        % <-- ajustar al puerto del RECEPTOR
duracion_s = 420;         % 7 min: unos 500 ensayos
archivoEntrada = "";      % para modo "archivo", por ejemplo "latencia_20261027_101500_rx.txt"

carpeta = fullfile("..", "evidencia", string(datetime("now", "Format", "yyyy-MM-dd")) + "_latencia");
if ~isfolder(carpeta), mkdir(carpeta); end

%% Registro
if modo == "serial"
    base = fullfile(carpeta, "latencia_" + string(datetime("now", "Format", "yyyyMMdd_HHmmss")));
    rx = serialport(puertoRx, 115200);
    setDTR(rx, false); setRTS(rx, false);   % evita reiniciar el ESP32 al abrir el puerto
    configureTerminator(rx, "LF");
    flush(rx);

    lineas = strings(0, 1);
    t0 = tic;
    ultimoAviso = 0;
    while toc(t0) < duracion_s
        while rx.NumBytesAvailable > 0
            lineas(end + 1, 1) = strtrim(readline(rx)); %#ok<SAGROW>
        end
        if toc(t0) - ultimoAviso > 30
            ultimoAviso = toc(t0);
            fprintf("%.0f s: %d ensayos\n", ultimoAviso, sum(startsWith(lineas, "L,")));
        end
        pause(0.01);
    end
    clear rx
    writelines(lineas, base + "_rx.txt");
else
    base = erase(archivoEntrada, "_rx.txt");
    lineas = strtrim(readlines(archivoEntrada));
end

%% Lectura
filasL = lineas(startsWith(lineas, "L,"));
L = nan(numel(filasL), 7);
for i = 1:numel(filasL)
    v = str2double(split(extractAfter(filasL(i), 2), ","));
    if numel(v) == 7, L(i, :) = v'; end
end
L = L(all(~isnan(L), 2), :);
nTimeouts = sum(startsWith(lineas, "T,"));

T = array2table(L, "VariableNames", ["n", "nivel", "secuencia", "llegada_us", ...
                                     "primer_us", "final_us", "pulsos_perdidos"]);
writetable(T, base + ".csv");

% Si se perdió algún pulso, el primer cambio pudo quedar 20 ms tarde: se descarta el ensayo
valido = T.pulsos_perdidos == 0 & T.llegada_us > 0;
fprintf("Ensayos: %d | válidos: %d | descartados: %d | timeouts: %d\n", ...
        height(T), sum(valido), sum(~valido), nTimeouts);
V = T(valido, :);

tramos = struct( ...
    "nombre", {"Tx + enlace (escalón -> paquete)", "Rx + PWM (paquete -> pulso)", ...
               "Punta a punta (escalón -> pulso)", "Hasta la orden final (con slew-rate)"}, ...
    "ms", {V.llegada_us / 1e3, (V.primer_us - V.llegada_us) / 1e3, ...
           V.primer_us / 1e3, V.final_us / 1e3});

%% Estadísticas
pct = @(x, p) interp1(linspace(0, 100, numel(x)), sort(x), p);
fprintf("\n%-38s %7s %7s %7s %7s %7s %7s\n", "Tramo (ms)", "media", "desv", "min", "mediana", "p95", "max");
for k = 1:numel(tramos)
    x = tramos(k).ms;
    fprintf("%-38s %7.2f %7.2f %7.2f %7.2f %7.2f %7.2f\n", tramos(k).nombre, ...
            mean(x), std(x), min(x), median(x), pct(x, 95), max(x));
end

% Filas para pegar en la tabla del informe
fprintf("\n%% Filas LaTeX (media, mediana, p95, máximo en ms)\n");
for k = 1:numel(tramos)
    x = tramos(k).ms;
    fprintf("%s & %.1f & %.1f & %.1f & %.1f \\\\\n", tramos(k).nombre, ...
            mean(x), median(x), pct(x, 95), max(x));
end

% Comparación por sentido del escalón (deberían dar parecido)
for s = 0:1
    x = V.primer_us(V.nivel == s) / 1e3;
    fprintf("Escalón hacia nivel %d: n=%d, mediana %.2f ms\n", s, numel(x), median(x));
end

%% Figuras
pe = tramos(3).ms;

f1 = figure("Color", "w", "Position", [100 100 800 420]);
histogram(pe, "BinWidth", 1, "FaceColor", [0.15 0.35 0.65]);
hold on
xline(median(pe), "-", sprintf("mediana %.1f ms", median(pe)), "LineWidth", 1.5, "Color", [0.85 0.33 0.10]);
xline(pct(pe, 95), "--", sprintf("p95 %.1f ms", pct(pe, 95)), "LineWidth", 1.5, "Color", [0.85 0.33 0.10]);
hold off
grid on
xlabel("Latencia de punta a punta [ms]"); ylabel("Cantidad de ensayos");
title(sprintf("Escalón en la entrada del índice -> pulso PWM del servo (n = %d)", numel(pe)));
set(gca, "FontSize", 12);
exportgraphics(f1, base + "_histograma.png", "Resolution", 200);

f2 = figure("Color", "w", "Position", [100 100 800 420]);
grupos = categorical(repelem(1:3, [numel(tramos(1).ms), numel(tramos(2).ms), numel(tramos(3).ms)])', ...
                     1:3, ["Tx + enlace", "Rx + PWM", "Punta a punta"]);
boxchart(grupos, [tramos(1).ms; tramos(2).ms; tramos(3).ms], "BoxFaceColor", [0.15 0.35 0.65]);
grid on
ylabel("Tiempo [ms]");
title("Reparto de la latencia por tramo");
set(gca, "FontSize", 12);
exportgraphics(f2, base + "_tramos.png", "Resolution", 200);

f3 = figure("Color", "w", "Position", [100 100 800 420]);
plot(1:height(V), pe, ".", "MarkerSize", 10, "Color", [0.15 0.35 0.65]);
grid on
xlabel("Ensayo válido (orden de registro)"); ylabel("Latencia de punta a punta [ms]");
title("Latencia a lo largo de la prueba");
set(gca, "FontSize", 12);
exportgraphics(f3, base + "_serie.png", "Resolution", 200);

fprintf("\nGuardado en %s (*.csv, *_rx.txt, *.png)\n", carpeta);
