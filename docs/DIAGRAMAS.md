# Funcionamiento en Diagramas

Versión gráfica y resumida del firmware `v2.3.0`. La explicación detallada de cada bloque está en
[`EXPLICACION_CODIGO.md`](../EXPLICACION_CODIGO.md).

> Los diagramas están en Mermaid: GitHub los dibuja automáticamente. En VS Code se ven con la extensión
> *Markdown Preview Mermaid Support*.

---

## 1. El sistema completo

```mermaid
flowchart LR
    subgraph G["🧤 GUANTE (Transmisor ESP32)"]
        P["5 potenciómetros<br/>(dedos)"]
        I["MPU6050<br/>(muñeca)"]
        TX["Calcula ángulos<br/>de servo"]
        P --> TX
        I --> TX
    end

    subgraph M["🤖 MANO (Receptor ESP32)"]
        RX["Valida y suaviza"]
        SD["5 servos SG90<br/>(dedos)"]
        SM["2 servos MG946R<br/>(muñeca)"]
        RX --> SD
        RX --> SM
    end

    TX -- "ESP-NOW 2,4 GHz<br/>19 bytes · 50 por segundo" --> RX
```

**Idea clave:** el guante hace todas las cuentas y manda los ángulos finales. La mano sólo valida,
suaviza y mueve.

---

## 2. Qué viaja por el aire

Un paquete de **19 bytes**, 50 veces por segundo:

```
┌────────┬─────┬─────┬─────┬─────┬─────┬─────┬──────────┬──────────┬──────────┐
│ 0xAA55 │ Sec │ Pul │ Índ │ Med │ Anu │ Meñ │ Muñeca V │ Muñeca R │ Checksum │
│ 2 B    │ 1 B │ 2 B │ 2 B │ 2 B │ 2 B │ 2 B │ 2 B      │ 2 B      │ 2 B      │
└────────┴─────┴─────┴─────┴─────┴─────┴─────┴──────────┴──────────┴──────────┘
  firma   n.º    25° = abierto … 90° = cerrado   25..90°    0..180°   Fletcher-16
```

```mermaid
flowchart LR
    A["Paquete recibido"] --> B{"¿Mide 19 bytes?"}
    B -- No --> X["🗑️ Descartar"]
    B -- Sí --> C{"¿Empieza con 0xAA55?"}
    C -- No --> X
    C -- Sí --> D{"¿Checksum correcto?"}
    D -- No --> X
    D -- Sí --> E["✅ Al buzón de la tarea de control"]
```

---

## 3. El guante: un ciclo cada 20 ms

```mermaid
flowchart TD
    S(["⏱️ Despertar cada 20 ms exactos"]) --> W1["Leer MPU6050<br/>acelerómetro + giróscopo"]
    W1 --> W2["Filtro complementario<br/>98% giróscopo + 2% acelerómetro"]
    W2 --> W3["Convertir a ángulos de servo<br/>pitch → 25..90° · roll → 0..180°"]
    W3 --> CAL{"¿Calibrando?"}
    CAL -- "Sí: no leer dedos,<br/>no enviar, no imprimir" --> S
    CAL -- No --> D1["Leer 5 dedos<br/>16 muestras promediadas c/u"]
    D1 --> D2["Convertir con la calibración<br/>abierta → 25° · cerrada → 90°"]
    D2 --> K["Armar paquete<br/>+ n.º de secuencia + checksum"]
    K --> E["📡 Enviar por ESP-NOW"]
    E --> T{"¿10.º ciclo?"}
    T -- Sí --> L["🖥️ Imprimir telemetría (5 Hz)"]
    T -- No --> S
    L --> S
```

### Cómo se obtiene cada dedo

```mermaid
flowchart LR
    A["Dedo se flexiona"] --> B["Tendón gira<br/>el potenciómetro"]
    B --> C["ADC: 16 lecturas<br/>→ promedio"]
    C --> D["Calibración<br/>min/máx guardada en flash"]
    D --> E["Ángulo 25°..90°"]
```

### Cómo se obtiene la muñeca

```mermaid
flowchart LR
    G["Giróscopo<br/>rápido pero deriva"] --> F["Filtro<br/>complementario"]
    A["Acelerómetro<br/>estable pero ruidoso"] --> F
    F --> P["Pitch<br/>(arriba / abajo)"]
    F --> R["Roll<br/>(rotación)"]
    P --> SV["Servo vertical<br/>+45° → 25 · 0° → 57 · −25° → 90"]
    R --> SR["Servo rotación<br/>−90° → 0 · 0° → 90 · +90° → 180"]
```

---

## 4. La mano: del paquete al servo

```mermaid
sequenceDiagram
    participant Aire as 📡 Radio
    participant CB as Callback ESP-NOW<br/>(tarea Wi-Fi)
    participant Buz as Buzón<br/>(cola de 1)
    participant Ctrl as Tarea de control
    participant Servo as Servos

    Aire->>CB: Llega un paquete
    CB->>CB: Validar tamaño, firma y checksum
    CB->>Buz: Guardar (pisa el anterior)
    Buz-->>Ctrl: Despierta en el acto (v2.2.0)
    Ctrl->>Ctrl: Nuevos objetivos (con límites mecánicos)
    Ctrl->>Ctrl: Rampa de velocidad + banda muerta
    Ctrl->>Servo: Nuevo ángulo (invertido si corresponde)
    Note over Ctrl: Si no llega nada en 20 ms,<br/>igual itera para el failsafe
```

### Cómo se mueve cada servo en cada ciclo

```mermaid
flowchart TD
    A["error = objetivo − posición actual"] --> B{"¿|error| > banda muerta?<br/>(1° dedos · 1,2° muñeca)"}
    B -- No --> Q["🤫 No hacer nada<br/>(evita zumbido)"]
    B -- Sí --> C["Avanzar como máximo VEL_MAX<br/>8° dedos · 4° y 5° muñeca"]
    C --> D{"¿Dedo invertido?"}
    D -- Sí --> E["Escribir 115° − posición"]
    D -- No --> F["Escribir posición"]
```

---

## 5. Seguridad: máquina de estados del receptor

```mermaid
stateDiagram-v2
    [*] --> EsperandoSync: Encendido (servos en Home)
    EsperandoSync --> Normal: Llega el 1.er paquete válido
    Normal --> Normal: Paquetes cada 20 ms
    Normal --> Failsafe: 250 ms sin paquetes
    Failsafe --> Normal: Vuelve la señal
    Failsafe --> Desacoplado: En Home y más de 2 s en failsafe
    Desacoplado --> Normal: Vuelve la señal (reacopla servos)

    note right of Failsafe
        Mano abierta (25°)
        Muñeca centrada (57° / 90°)
    end note
    note right of Desacoplado
        Sin PWM: servos sin corriente
        ni calentamiento
    end note
```

> "Desacoplado" no es un estado del código: es el failsafe con los servos apagados (`detach`).
> Se dibuja aparte para que se entienda qué pasa.

---

## 6. Calibración de los dedos

```mermaid
flowchart TD
    A["Enviar 'c' por el monitor serie"] --> P["⏸️ Se pausan envío y telemetría<br/>la mano robot va a Home (failsafe)"]
    P --> B["✋ Abrir la mano<br/>y apretar ENTER"]
    B --> C["Guarda el mínimo de cada dedo"]
    C --> D["✊ Cerrar el puño<br/>y apretar ENTER"]
    D --> E["Guarda el máximo de cada dedo"]
    E --> F["💾 Escribe en flash (NVS)<br/>queda guardado aunque se apague"]
    F --> R["▶️ Se reanuda el envío<br/>la mano vuelve a seguir al guante"]
```

La muñeca se calibra sola al encender el guante: **hay que mantener la mano quieta ~1,5 s.**

---

## 7. ¿Cuánto tarda? (estimado, v2.2.0)

```mermaid
flowchart LR
    A["🖐️ t = 0<br/>el operador<br/>se mueve"] -- "~10 ms<br/>espera al ciclo" --> B["🧤 t ≈ 10<br/>el guante<br/>lee"]
    B -- "~5 ms<br/>filtro + cálculo" --> C["📡 t ≈ 15<br/>sale el<br/>paquete"]
    C -- "~3 ms<br/>radio" --> D["🤖 t ≈ 18<br/>la mano<br/>lo procesa"]
    D -- "~10 ms<br/>próximo pulso PWM" --> E["⚙️ t ≈ 30 ms<br/>el servo recibe<br/>la orden"]
```

≈ **30 ms** de media hasta la orden, más lo que tarda el servo en recorrer el ángulo.
Son estimaciones a partir del código, no mediciones.

---

## 8. Historia de versiones

```mermaid
timeline
    title Evolución del firmware
    Preliminar : Doble filtro EMA
               : Bucle con delay(20)
    v1.0.0 : Filtro de Kalman en la muñeca
           : Deriva tipo "mouse"
    v2.0.0 : Filtro complementario
           : Calibración del giróscopo
    v2.1.0 : Servos más rápidos
           : Dedos invertibles
    v2.2.0 : Receptor despierta al llegar el paquete
           : DLPF 44 Hz
           : Validada en hardware
    v2.3.0 : Calibración sin transmisión
           : Consola limpia al calibrar
```

---

## 9. ¿Qué tocar si…?

| Si pasa esto… | …tocar esto | En |
|---|---|---|
| 🔄 Un dedo va al revés | `INVERTIR_DEDO` | Mano |
| ✋ Un dedo no abre/cierra del todo | Recalibrar con `c` | Guante |
| 💥 Movimientos bruscos o reinicios | Bajar `VEL_MAX_*` | Mano |
| 🐢 Movimientos lentos | Subir `VEL_MAX_*` | Mano |
| 🐝 Servos zumban quietos | Subir `DEADBAND_*` | Mano |
| 📐 Muñeca no centrada | `TRIM_PITCH_DEG` / `TRIM_ROLL_DEG` | Guante |
| 📶 `RF:NO_ACK` | Revisar `macReceptor[]` | Guante |
