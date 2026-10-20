%% Latencia Tx -> Rx medida por el propio receptor (sin osciloscopio)
% Requiere: ambos firmwares (rama exp/medicion-latencia) con MEDIR_LATENCIA = 1, TELEMETRIA_MATLAB = 0,
% y un cable del GPIO23 del transmisor al GPIO34 del receptor, más un cable GND-GND.
% Formato de las líneas del receptor:
%   L,seq,radio_us,total_us
%     radio_us: inicio del ciclo del Tx -> llegada del paquete al callback ESP-NOW del Rx
%     total_us: inicio del ciclo del Tx -> servos actualizados en el Rx
% Las demás líneas (telemetría de 5 Hz, arranque) se ignoran.
%
% Requiere MATLAB R2020a o posterior. Ejemplo: no fue probado.

puertoRx = "COM6";   % <-- ajustar
duracion_s = 60;     % tiempo de registro
archivo = "latencia_" + string(datetime("now", "Format", "yyyyMMdd_HHmmss"));

%% Registro
rx = serialport(puertoRx, 115200);
setDTR(rx, false); setRTS(rx, false);   % evita reiniciar el ESP32
configureTerminator(rx, "LF");
flush(rx);

lineas = strings(0, 1);
t0 = tic;
while toc(t0) < duracion_s
    while rx.NumBytesAvailable > 0
        lineas(end + 1, 1) = strtrim(readline(rx)); %#ok<SAGROW>
    end
    pause(0.005);
end
clear rx
writelines(lineas, archivo + "_rx.txt");

%% Análisis
lineas = lineas(startsWith(lineas, "L,"));
L = nan(numel(lineas), 3);
for i = 1:numel(lineas)
    v = str2double(split(extractAfter(lineas(i), 2), ","));
    if numel(v) == 3, L(i, :) = v'; end
end
L = L(all(~isnan(L), 2), :);

% Una latencia mayor a 40 ms (dos ciclos) significa que se emparejó mal el flanco
% (cable suelto o flanco perdido): se descarta.
validas = L(:, 3) < 40000;
fprintf("Paquetes: %d | descartados (> 40 ms): %d\n", size(L, 1), sum(~validas));
L = L(validas, :);

nombres = ["Radio (Tx inicio de ciclo -> callback Rx)", "Total (Tx inicio de ciclo -> servos escritos)"];
for c = 2:3
    x = L(:, c) / 1000;   % ms
    fprintf("%s: min %.2f | media %.2f | p95 %.2f | max %.2f ms\n", nombres(c - 1), ...
            min(x), mean(x), prctile(x, 95), max(x));
end
writematrix(L, archivo + "_L.csv");

figure("Name", "Latencia Tx -> Rx");
histogram(L(:, 3) / 1000, "BinWidth", 0.25); hold on;
histogram(L(:, 2) / 1000, "BinWidth", 0.25);
xlabel("Latencia (ms)"); ylabel("Paquetes"); grid on;
legend(["Total (hasta servos)", "Radio (hasta callback)"]);
