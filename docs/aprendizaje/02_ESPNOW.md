# 2. ESP-NOW: qué es y cómo lo usamos

[← Volver al índice](README.md)

## 2.1. Qué es

**ESP-NOW** es un protocolo de Espressif (el fabricante del ESP32) para mandar **paquetes cortos
directamente de una placa a otra**, usando la misma radio Wi-Fi de 2,4 GHz que ya trae el chip.

Lo que lo hace distinto de "usar Wi-Fi":

| Wi-Fi común (p. ej. UDP) | ESP-NOW |
|---|---|
| Hace falta un router (o que una placa haga de punto de acceso) | No hace falta router ni red |
| Hay que asociarse a la red y obtener IP (DHCP): tarda segundos | No hay conexión: se manda y listo |
| Si se cae la red, hay que reconectar | No hay nada que reconectar; el próximo paquete simplemente llega |
| Pasa por la pila IP/UDP (más capas, más procesamiento) | Va directo en una trama de Wi-Fi, sin IP |
| Se direcciona por IP | Se direcciona por **dirección MAC** |

Cómo funciona por dentro, en una frase: ESP-NOW mete nuestros datos en una **trama de acción propia del
fabricante** (*vendor-specific action frame*) del estándar IEEE 802.11. Para la radio es una trama Wi-Fi
más, pero no pertenece a ninguna red.

Límites que conviene conocer:

- **Hasta 250 bytes por paquete** (versión clásica). El nuestro usa 19.
- **Hasta 20 *peers*** registrados (con cifrado, menos).
- **Las dos placas tienen que estar en el mismo canal** de Wi-Fi.

> **Frase para la defensa:** "Elegimos ESP-NOW porque usa la radio que el ESP32 ya tiene, no necesita
> router ni conexión previa, y está pensado para paquetes cortos y frecuentes como los nuestros: 19 bytes
> cada 20 ms."

### Por qué no las otras opciones

- **Wi-Fi con UDP/TCP:** necesita una red (router o *softAP*), asociación y dirección IP. Agrega capas y
  un tiempo de conexión al arrancar o tras una caída. No aporta nada que necesitemos (no vamos a internet).
- **Bluetooth (clásico o BLE):** requiere emparejar y mantener una conexión; en BLE los datos salen en
  "intervalos de conexión" negociados, lo que agrega latencia y complejidad.
- **Un módulo de RF aparte (p. ej. nRF24L01):** más hardware, más cables y otra librería, cuando el ESP32
  ya trae una radio.

---

## 2.2. Conceptos clave

### Dirección MAC y *peer*

Cada ESP32 tiene una **MAC** de 6 bytes grabada de fábrica. Para mandar un paquete a una placa concreta
(*unicast*) hay que registrarla antes como **peer** con `esp_now_add_peer()`. Para **recibir** no hace
falta registrar a nadie.

### Envío asíncrono y *callback* de envío

`esp_now_send()` **no espera** a que el paquete llegue: lo deja en manos de la radio y retorna enseguida.
Más tarde, el sistema llama a nuestra función de *callback* (`OnDataSent`) con el resultado.

### ACK

En unicast, la radio del receptor devuelve automáticamente un **acuse de recibo (ACK)** a nivel de la
capa MAC. Si llegó, el callback informa `ESP_NOW_SEND_SUCCESS`.

Ojo: **el ACK confirma que la radio del receptor recibió la trama, no que nuestro programa la procesó.**
Si el paquete llegó pero el checksum no valida, igual hubo ACK.

### *Callback* de recepción

Cuando llega un paquete, el sistema llama a nuestra función `OnDataRecv` con los bytes. **Corre dentro de
la tarea de Wi-Fi** (núcleo 0, prioridad alta), por eso Espressif recomienda que sea muy corta: nada de
`Serial.print`, esperas ni cálculos largos. Nosotros sólo validamos y copiamos a una cola (ver
[FreeRTOS, sección 1.4](01_FREERTOS.md#14-cómo-lo-usamos-en-el-receptor)).

### Canal

La radio de 2,4 GHz tiene varios canales. Si ninguna placa está conectada a un router, ambas quedan en el
mismo canal por defecto y se entienden. Si una se conectara a un router en otro canal, dejarían de
comunicarse. Por eso no conectamos ninguna placa a una red.

### Cifrado

ESP-NOW permite cifrar con claves (PMK/LMK). **No lo usamos** (`encrypt = false`): agrega configuración de
claves en ambos lados y reduce la cantidad de peers. Para un prototipo con un solo guante es razonable;
para un uso real habría que activarlo (ver preguntas).

---

## 2.3. Cómo lo usamos: transmisor

### Arranque (en `setup()`)

```cpp
WiFi.mode(WIFI_STA);        // 1. encender la radio en modo estación
WiFi.setSleep(false);       // 2. desactivar el ahorro de energía de la radio
esp_now_init();             // 3. iniciar ESP-NOW
esp_now_register_send_cb(OnDataSent);   // 4. registrar el callback de envío

memcpy(infoReceptor.peer_addr, macReceptor, 6);   // 5. registrar al receptor como peer
infoReceptor.channel = 0;      // 0 = "el canal en el que esté la radio ahora"
infoReceptor.encrypt = false;
esp_now_add_peer(&infoReceptor);
```

Por qué cada paso:

1. **`WIFI_STA`:** ESP-NOW necesita la radio Wi-Fi encendida. El modo estación la enciende **sin**
   conectarse a ninguna red y sin crear un punto de acceso.
2. **`setSleep(false)`:** por defecto la radio entra en un modo de ahorro en el que se apaga a ratos. Eso
   puede demorar o perder paquetes. Como el guante manda 50 por segundo, preferimos la radio siempre
   despierta, a cambio de un poco más de consumo.
3. **`esp_now_init()`:** inicializa el protocolo. Si falla, se informa por serie.
4. **Callback de envío:** sólo actualiza `ultimoEnvioExitoso`, que la telemetría muestra como `OK` o
   `NO_ACK`. Es la forma más barata de saber, desde el monitor serie, si la mano está recibiendo.
5. **Peer:** la MAC del receptor está escrita en el código (`macReceptor[]`). Se obtiene del propio
   receptor, que la imprime al arrancar (`WiFi.macAddress()`).

### El `#if ESP_IDF_VERSION`

```cpp
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
void OnDataSent(const wifi_tx_info_t* info, esp_now_send_status_t status) { ... }
#else
void OnDataSent(const uint8_t* mac_addr, esp_now_send_status_t status) { ... }
#endif
```

Espressif cambió los parámetros de los callbacks entre versiones del ESP-IDF (que corresponden a las
versiones 2.x y 3.x del core Arduino para ESP32). El `#if` elige la firma correcta en tiempo de
compilación para que el mismo código compile con ambas.

### Envío (cada 20 ms, en la tarea)

```cpp
esp_now_send(macReceptor, reinterpret_cast<uint8_t*>(&paqueteSalida), sizeof(paqueteSalida));
```

- Se manda la estructura **tal cual está en memoria** (19 bytes). `reinterpret_cast` sólo le dice al
  compilador "tratá esta estructura como un arreglo de bytes".
- **No reenviamos** si falla: 20 ms después sale un paquete nuevo con datos más actuales. Reenviar el viejo
  sólo agregaría retardo.

### Un efecto lateral de la radio: el ADC2

El ESP32 tiene dos conversores analógico-digitales. **El ADC2 no se puede usar con la radio Wi-Fi
encendida**, y ESP-NOW la necesita encendida. Por eso los cinco potenciómetros van a pines del **ADC1**
(GPIO 32, 33, 34, 35 y 39).

---

## 2.4. Cómo lo usamos: receptor

### Arranque

```cpp
WiFi.mode(WIFI_STA);
WiFi.setSleep(false);
Serial.println(WiFi.macAddress());    // la MAC que hay que copiar en el transmisor
esp_now_init();
esp_now_register_recv_cb(OnDataRecv);
```

El receptor **no registra peers**: no le manda nada a nadie, sólo escucha.

### El callback de recepción, paso a paso

```cpp
void OnDataRecv(..., const uint8_t* incomingData, int len) {
    if (len != sizeof(MensajeBrazo)) return;              // 1. tamaño
    const MensajeBrazo* paquete = reinterpret_cast<const MensajeBrazo*>(incomingData);
    if (!validarPaqueteBrazo(*paquete)) return;            // 2. firma + checksum
    xQueueOverwriteFromISR(colaMensajesBrazo, paquete, ...);   // 3. al buzón
}
```

1. **Tamaño:** si no son exactamente 19 bytes, no es nuestro paquete. Es el filtro más barato.
2. **Firma `0xAA55` y checksum Fletcher-16:** descartan paquetes corruptos o de otro dispositivo ESP-NOW
   cercano que justo mande 19 bytes. La capa Wi-Fi ya tiene su propio CRC-32; esto es una segunda barrera
   a nivel de nuestra aplicación.
3. **Cola:** se copia el paquete y se sale. Todo el trabajo pesado lo hace la tarea de control en el
   núcleo 1.

### Qué pasa si se pierden paquetes

ESP-NOW **no garantiza la entrega** y nosotros no retransmitimos. El diseño lo tolera así:

- **Uno o pocos perdidos:** la mano sigue yendo hacia el último objetivo recibido; el siguiente paquete
  llega 20 ms después, así que el efecto es mínimo.
- **Más de 250 ms sin paquetes válidos** (unos 12 seguidos): la máquina de estados entra en **failsafe**
  y lleva la mano a Home. A los 2 s en Home, desacopla los servos.
- **Vuelve la señal:** con el primer paquete válido, vuelve a operación normal.

---

## 2.5. Resumen de funciones de ESP-NOW y Wi-Fi usadas

| Función | Dónde | Para qué |
|---|---|---|
| `WiFi.mode(WIFI_STA)` | Ambos | Encender la radio sin conectarse a ninguna red |
| `WiFi.setSleep(false)` | Ambos | Desactivar el ahorro de energía de la radio (menos latencia y pérdidas) |
| `WiFi.macAddress()` | Rx | Imprimir la MAC para copiarla en el transmisor |
| `esp_now_init()` | Ambos | Inicializar ESP-NOW |
| `esp_now_register_send_cb()` | Tx | Registrar `OnDataSent` (resultado de cada envío) |
| `esp_now_add_peer()` | Tx | Registrar la MAC del receptor como destino |
| `esp_now_send()` | Tx | Enviar los 19 bytes (asíncrono) |
| `esp_now_register_recv_cb()` | Rx | Registrar `OnDataRecv` (llegada de cada paquete) |

---

## 2.6. Preguntas de práctica

**¿Qué es ESP-NOW?**
Un protocolo de Espressif para mandar paquetes cortos entre ESP32 directamente, por MAC, usando la radio
Wi-Fi pero sin router, sin conexión y sin IP.

**¿Por qué no Wi-Fi común o Bluetooth?**
Wi-Fi necesita una red, asociación e IP, y reconectar si se cae. Bluetooth necesita emparejar y mantener
una conexión. ESP-NOW no necesita nada de eso y está pensado para paquetes chicos y frecuentes.

**¿Qué alcance tiene?**
Depende del entorno y de las antenas. No lo medimos; es una de las cosas que quedan por caracterizar.

**¿Cuánto tarda un paquete en llegar?**
Los valores típicos reportados para ESP-NOW son de unos pocos milisegundos. La cadena completa (del
potenciómetro al pulso PWM del servo) la medimos en la prueba P11: unos **24 ms de media**.

**¿Cómo sabe el transmisor si el paquete llegó?**
Por el ACK que devuelve la radio del receptor: el callback `OnDataSent` recibe `SUCCESS` o falla, y la
telemetría lo muestra como `OK` o `NO_ACK`. El ACK confirma que llegó a la radio, no que pasó el checksum.

**¿Qué pasa si se pierde un paquete?**
Nada visible: 20 ms después llega el siguiente con datos más nuevos. No reenviamos porque el dato viejo
ya no sirve. Si se pierden durante más de 250 ms, la mano entra en failsafe.

**¿Para qué sirve el número de secuencia?**
Va en cada paquete (0 a 255 y vuelve a empezar). Hoy el receptor sólo lo muestra al establecer el enlace;
no lo usa para contar pérdidas. Sería la base para medir la tasa de paquetes perdidos.

**¿Por qué el checksum si Wi-Fi ya tiene CRC?**
El CRC de Wi-Fi protege la trama en el aire. El nuestro valida a nivel de aplicación que esos 19 bytes
son un paquete nuestro bien armado, por ejemplo frente a otro dispositivo ESP-NOW cercano.

**¿Y si otro ESP32 manda paquetes a la mano?**
Hoy el receptor no filtra por MAC de origen: si alguien conoce el formato (firma y checksum), la mano lo
aceptaría. Para un prototipo es aceptable; las mejoras serían filtrar por la MAC del guante y activar el
cifrado de ESP-NOW.

**¿Por qué la MAC del receptor está fija en el código?**
Porque es lo más simple con un solo receptor. La contra es que si se cambia la placa hay que copiar la
nueva MAC y reprogramar el guante. Una alternativa sería un emparejamiento inicial por *broadcast*.

**¿Por qué los potenciómetros están en esos pines?**
Porque son del ADC1. El ADC2 no funciona mientras la radio Wi-Fi está encendida, y ESP-NOW la necesita.

**¿Por qué `WiFi.setSleep(false)`?**
Para que la radio no entre en ahorro de energía, que puede demorar o perder paquetes. El costo es algo más
de consumo en ambas placas.
