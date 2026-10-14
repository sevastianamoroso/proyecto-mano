%% Telemetría Tx/Rx alineada por número de paquete
% Requiere flashear ambos firmwares (rama exp/medicion-latencia) con TELEMETRIA_MATLAB = 1.
% Formato de las líneas:
%   T,millis,seq,d1,d2,d3,d4,d5,V,R                      (transmisor, una por paquete enviado)
%   R,millis,seq,nuevo,estado,d1,d2,d3,d4,d5,V,R         (receptor, una por ciclo de control)
% Las líneas que no empiezan con "T," o "R," (mensajes de arranque, etc.) se ignoran.
%
% Cómo se alinea: cada paquete del transmisor se ubica en el instante (reloj del receptor) en que
% el receptor lo recibió (fila con nuevo = 1 y la misma secuencia). Así las dos curvas quedan en
% el mismo reloj y no influye la demora del USB. El corrimiento entre la orden (Tx) y la posición
% (Rx) es el retraso que agrega el limitador de velocidad del receptor, no la latencia de radio
% (esa se mide con el osciloscopio, prueba P11).
%
% Requiere MATLAB R2020a o posterior (serialport, setDTR, setRTS). Ejemplo: no fue probado.

puertoTx = "COM5";   % <-- ajustar
puertoRx = "COM6";   % <-- ajustar
duracion_s = 20;     % tiempo de registro
archivo = "telemetria_" + string(datetime("now", "Format", "yyyyMMdd_HHmmss"));

%% Registro
tx = serialport(puertoTx, 115200);
setDTR(tx, false); setRTS(tx, false);   % evita reiniciar el ESP32 (ver nota abajo)
rx = serialport(puertoRx, 115200);
setDTR(rx, false); setRTS(rx, false);
configureTerminator(tx, "LF"); configureTerminator(rx, "LF");
flush(tx); flush(rx);

lineasTx = strings(0, 1);
lineasRx = strings(0, 1);
t0 = tic;
while toc(t0) < duracion_s
    while tx.NumBytesAvailable > 0
        lineasTx(end + 1, 1) = strtrim(readline(tx)); %#ok<SAGROW>
    end
    while rx.NumBytesAvailable > 0
        lineasRx(end + 1, 1) = strtrim(readline(rx)); %#ok<SAGROW>
    end
    pause(0.005);
end
% Nota: si al cerrar los puertos las placas se reinician, la mano va a reposo. No afecta los datos
% ya registrados. Para evitarlo, dejar los puertos abiertos entre registros.
clear tx rx

writelines(lineasTx, archivo + "_tx.txt");
writelines(lineasRx, archivo + "_rx.txt");
if any(contains([lineasTx; lineasRx], "[SISTEMA] Iniciando"))
    warning("Alguna placa se reinició durante el registro (apareció el mensaje de arranque).");
end

%% Lectura de las líneas CSV
T = parsear(lineasTx, "T,", 10);   % millis, seq, d1..d5, V, R
R = parsear(lineasRx, "R,", 12);   % millis, seq, nuevo, estado, d1..d5, V, R

% La secuencia es de 8 bits (0..255): se "desenrolla" para que sea creciente.
seqTx = desenrollar(T(:, 2));

% Paquetes procesados por el receptor (filas con nuevo = 1), con la secuencia desenrollada y
% llevada a la numeración del Tx. Se asume que ambos registros empiezan juntos (la secuencia da
% una vuelta cada 256 paquetes = 5,12 s).
llegadas = R(R(:, 3) == 1, :);
seqRx = desenrollar(llegadas(:, 2));
k0 = find(T(:, 2) == llegadas(1, 2), 1);
seqRx = seqRx - seqRx(1) + seqTx(k0);

% Instante (reloj del Rx) en que se procesó cada paquete del Tx; NaN si no se procesó
[esta, pos] = ismember(seqTx, seqRx);
tLlegada = nan(size(T, 1), 1);
tLlegada(esta) = llegadas(pos(esta), 1);
perdidos = sum(~esta(k0:end));
fprintf("Paquetes enviados: %d | procesados por el Rx: %d | no procesados: %d (%.1f %%)\n", ...
        numel(esta(k0:end)), sum(esta(k0:end)), perdidos, 100 * perdidos / numel(esta(k0:end)));
% "No procesado" incluye paquetes perdidos por radio y paquetes que el receptor sobrescribió en
% la cola porque llegaron dos en el mismo ciclo de control.
fprintf("Saltos de secuencia en el Tx: %d\n", sum(diff(seqTx) ~= 1));

%% Gráficos (eje de tiempo: reloj del receptor, en segundos desde el inicio)
tRef = R(1, 1);
nombres = ["Pulgar", "Índice", "Medio", "Anular", "Meñique", "Muñeca vertical", "Muñeca rotación"];
colTx = 3:9;    % columnas de T con d1..d5, V, R
colRx = 5:11;   % columnas de R con d1..d5, V, R
figure("Name", "Telemetría Tx/Rx");
tiledlayout(4, 2, "TileSpacing", "compact");
for c = 1:7
    nexttile;
    plot((tLlegada - tRef) / 1000, T(:, colTx(c)), ".-"); hold on;
    plot((R(:, 1) - tRef) / 1000, R(:, colRx(c)), "-", "LineWidth", 1.2);
    title(nombres(c)); ylabel("°"); grid on;
end
xlabel("Tiempo (s)");
legend(["Orden recibida (Tx)", "Posición del servo (Rx)"], "Location", "best");

%% Funciones auxiliares
function M = parsear(lineas, prefijo, nCampos)
    lineas = lineas(startsWith(lineas, prefijo));
    M = nan(numel(lineas), nCampos);
    for i = 1:numel(lineas)
        v = str2double(split(extractAfter(lineas(i), 2), ","));
        if numel(v) == nCampos && all(~isnan(v))
            M(i, :) = v';
        end
    end
    M = M(all(~isnan(M), 2), :);   % descarta líneas cortadas o corruptas
end

function s = desenrollar(seq)
    d = mod(diff(seq), 256);
    s = [seq(1); seq(1) + cumsum(d)];
end
