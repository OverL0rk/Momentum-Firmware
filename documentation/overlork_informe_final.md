# OverL0rk Security Suite — Informe Final
**Flipper Zero × Momentum Firmware**  
**Autor:** Eudys Ramirez (`@OverL0rk`)  
**Fecha:** Mayo 2026  
**Status:** 🔒 Local — no publicar hasta autorización expresa

---

## Índice
1. [¿Qué es el OverL0rk Suite?](#1-qué-es-el-overlork-suite)
2. [Resumen ejecutivo de todo lo construido](#2-resumen-ejecutivo)
3. [Detalle por componente](#3-detalle-por-componente)
4. [Cómo funciona cada pieza](#4-cómo-funciona-cada-pieza)
5. [Cosas curiosas y detalles técnicos](#5-cosas-curiosas)
6. [Investigación externa — ideas de la comunidad](#6-investigación-externa)
7. [Ideas implementables del ecosistema](#7-ideas-implementables)
8. [Firma personal — OverL0rk ID](#8-firma-personal)

---

## 1. ¿Qué es el OverL0rk Suite?

El **OverL0rk Security Suite** es un conjunto de bibliotecas compartidas y aplicaciones FAP
(Flipper App Package) integradas en el Momentum Firmware para el Flipper Zero, que convierten
el dispositivo en un **monitor de seguridad pasivo, auditador de eventos y analizador forense**.

No es un firmware alternativo — es una **capa de inteligencia** que se monta encima del
firmware oficial sin romper ninguna funcionalidad existente.

**Filosofía de diseño:**
> "Registra todo silenciosamente, analiza sin interferir, alerta cuando importa."

---

## 2. Resumen Ejecutivo

### Snapshot rápido

| Categoría | Cantidad | Nombres |
|-----------|----------|---------|
| Bibliotecas compartidas | **6** | audit, overlork_events, overlork_rules, overlork_notify, overlork_protocol_id, overlork_crypto |
| FAP (aplicaciones) | **6** | audit_viewer, pattern_analyzer, overlork_dashboard, flipper_guard, rf_scanner, overlork_id |
| Hooks de firmware | **6** | NFC, SubGhz, IR, BadKB, LFRFID, iButton |
| Archivos modificados (firmware core) | **6** | 1 por subsistema |
| Líneas de código nuevas (aprox.) | **~3,500** | C + Python (SConscript) |
| Consumo de RAM adicional en idle | **~0 KB** | todo es on-demand |
| Impacto en batería | **< 0.1%** | hooks son funciones de 10–20 ciclos |

### Timeline de construcción (fases)

```
Fase 1 ──► lib/audit + hooks NFC/SubGhz/IR/BadKB + audit_viewer
Fase 2 ──► overlork_events + overlork_rules + overlork_notify + flipper_guard v0.2
Fase 3 ──► pattern_analyzer v0.2 + overlork_dashboard v0.1 + rf_scanner v0.1
Fase 4 ──► overlork_protocol_id + overlork_crypto + rf_scanner watch mode
Fase 5 ──► Hooks LFRFID + iButton + overlork_dashboard v0.2 + overlork_id
```

---

## 3. Detalle por Componente

### 3.1 Bibliotecas Compartidas (`lib/`)

#### `lib/audit` — El Notario
**¿Qué hace?**  
Escribe una línea CSV en la tarjeta SD cada vez que cualquier subsistema captura un evento.
Crea el archivo si no existe, añade un header, y hace un `fflush` atómico.

**¿Cómo?**  
`audit_log_event("NFC", "READ", uid, protocol)` → abre `/ext/audit/audit-2026-05-20.csv`,
appends `2026-05-20 14:32:11,NFC,READ,04:AB:CD:EF,NTAG215\n`, cierra.

**API clave:**
```c
void audit_log_event(
    const char* subsystem,   // "NFC", "SubGhz", "IR", "BadKB", "LFRFID", "iButton"
    const char* operation,   // "READ", "RX", "TX", "RUN", "SCAN"
    const char* identifier,  // UID, frecuencia, nombre de archivo
    const char* details      // protocolo, RSSI, etc.
);
```

---

#### `lib/overlork_events` — El Traductor
**¿Qué hace?**  
Convierte una línea de texto CSV en un struct tipado `OlEvent`. Entiende todos los
subsistemas y tiene tolerancia de capitalizaciones (`SubGhz` vs `SubGHz`).

**¿Cómo?**  
`ol_event_from_csv_line(char* line, OlEvent* out)` tokeniza los 5 campos con `strchr`/NUL
in-place, convierte el timestamp a epoch UNIX con la API `datetime_datetime_to_timestamp`.

**Tipos de eventos:**
```
OlEventTypeNfcRead       — lector de tarjetas contactless
OlEventTypeSubGhzRx      — señal de radio recibida y decodificada
OlEventTypeIrTx          — transmisión infrarroja
OlEventTypeIrRx          — recepción infrarroja
OlEventTypeBadKbRun      — script de teclado ejecutado
OlEventTypeLfRfidRead    — tarjeta RFID de baja frecuencia leída
OlEventTypeIButtonRead   — llave iButton / 1-Wire leída
OlEventTypeOther         — catch-all / wildcard en reglas
```

---

#### `lib/overlork_rules` — El Juez
**¿Qué hace?**  
Evalúa un `OlEvent` contra una tabla de reglas integrada y devuelve la regla de mayor
severidad que aplique.

**¿Cómo?**  
Recorre la tabla `g_rules[]` de índice 0→N. Cada regla tiene filtros de tipo, subsistema y
operación (NULL = wildcard). Guarda la mejor coincidencia por orden de severidad.

**Tabla de reglas integrada:**

| Regla | Tipo de evento | Severidad |
|-------|---------------|-----------|
| Any event (catch-all) | * | Info |
| NFC card read | NfcRead | ⚠️ Warn |
| SubGhz signal | SubGhzRx | 🚨 Alert |
| IR transmission | IrTx | ⚠️ Warn |
| **BadKB script run** | BadKbRun | 🔴 **Critical** |
| LFRFID card read | LfRfidRead | ⚠️ Warn |
| iButton key read | IButtonRead | ⚠️ Warn |

**¿Por qué BadKB es Critical?** Porque es el único subsistema que ejecuta código
arbitrario en el host — los demás solo capturan señales pasivamente.

---

#### `lib/overlork_notify` — El Mensajero
**¿Qué hace?**  
Traduce una severidad a un patrón de LED + vibración en el hardware del Flipper.

| Severidad | Color LED | Vibración |
|-----------|-----------|-----------|
| Info | 🔵 Cyan | No |
| Warn | 🟡 Yellow | No |
| Alert | 🔴 Red | No |
| Critical | 🟣 Magenta | **Sí** |

**¿Por qué vibración solo para Critical?** Para que el dispositivo en el bolsillo
alerte al operador de un evento de alto riesgo sin tener que mirar la pantalla.

---

#### `lib/overlork_protocol_id` — El Detective de Radio
**¿Qué hace?**  
Identifica el tipo de modulación OOK de un array de duraciones de pulso sin usar
memoria dinámica.

**¿Cómo?** Algoritmo en 4 pasos:
1. **Fast-path NEC:** si `durations[0] > 7 ms` → es líder NEC (9ms on / 4.5ms off)
2. **Histograma:** 100 bins de 50µs → pico más alto = chip time (1T)
3. **Fit test:** ±25% de 1T y 2T → porcentaje de pulsos que encajan
4. **PWM vs Manchester:** si >20% de gaps son 2T → Manchester biphase

```c
OlProtoResult result;
if(ol_protocol_id(pulses, count, &result)) {
    // result.type     = OlProtoPwmOok / OlProtoManchesterOok / OlProtoNec
    // result.confidence = 0-100
    // result.chip_time_us = ancho del pulso base en µs
}
```

**Curiosidad:** No usa `malloc`. Todo el estado cabe en ~256 bytes de stack.

---

#### `lib/overlork_crypto` — El Cofre
**¿Qué hace?**  
Wraps limpio sobre mbedtls y el hardware TRNG/AES-GCM del STM32WB55.

- `ol_crypto_sha256()` — hash SHA-256 via mbedtls
- `ol_crypto_hmac_sha256()` — HMAC para integridad de logs
- `ol_crypto_aes_gcm_encrypt/decrypt()` — cifrado autenticado AES-256-GCM
- `ol_crypto_random_bytes()` — TRNG hardware del chip

**Curiosidad:** `mbedtls_sha256_starts(&ctx, 0)` — el `0` es el flag "no es SHA-224".
Si lo pones en `1`, obtienes SHA-224 silenciosamente sin error. Un bug clásico.

---

### 3.2 Aplicaciones FAP (`applications_user/`)

#### `audit_viewer` — La Lupa
**¿Qué hace?** Lee y muestra los últimos 2 KB del CSV de hoy en un TextBox.
**Para qué sirve?** Ver rápidamente "¿qué pasó en los últimos minutos?" sin necesidad
de sacar la SD o conectar USB.  
**Impacto:** ~0 KB de RAM cuando no está abierto. Lee el archivo on-demand.

---

#### `pattern_analyzer` — El Forense
**¿Qué hace?** Carga el CSV del día y ejecuta 7 algoritmos de detección de patrones:

| Algoritmo | Detecta |
|-----------|---------|
| `sequential_uids` | UIDs NFC consecutivos → posible clonación |
| `shared_prefix` | Múltiples UIDs con mismo prefijo → misma familia de tarjetas |
| `repeat_exposure` | Mismo ID ≥3 veces → reconocimiento repetido |
| `analyze_burst` | ≥5 eventos en ≤5 segundos → actividad anómala |
| `analyze_short_burst` | ≥3 eventos en cualquier ventana de 5s |
| `analyze_cross_protocol` | Mismo ID en subsistemas diferentes → reconocimiento multi-vector |
| `analyze_top_identifiers` | Los 2 IDs más frecuentes del día |

**Export:** genera `/ext/audit/report-YYYY-MM-DD.txt` con los hallazgos.

**Curiosidad técnica:** `analyze_top_identifiers` usa `malloc` para los arrays de conteo
en lugar de variables locales. ¿Por qué? Porque el stack de los FAPs en Flipper tiene
un límite de **2 KB totales**. Un array de 256 entradas de 8 bytes = 2 KB exactos →
stack overflow garantizado si lo pones en el stack.

---

#### `overlork_dashboard` — El Panel de Mando
**¿Qué hace?** Dashboard de resumen estadístico con 3 escenas:
- **Stats:** contadores por subsistema (7 subsistemas), último evento, batería
- **Log:** últimos 2 KB del CSV como TextBox
- **About:** créditos del autor

**v0.2 — Status-bar indicator:**  
Un `ViewPort` registrado en `GuiLayerStatusBarRight` muestra el total de eventos
del día en la barra de estado del sistema (siempre visible, incluso en otras apps).
Muestra `"42"` o `"99+"`.

```
┌─ Barra de estado de Flipper ───────────────────── [42] ─┐
│  Sat  14:32    BT: off    SD: ok    Bat: 87%    [ 42 ]  │
└─────────────────────────────────────────────────────────┘
```

**Curiosidad:** `widget_add_string_element()` guarda un **puntero** a la cadena, no
una copia. Si el buffer se destruye antes de que el Widget lo dibuje → garbage en pantalla
o crash. Por eso todos los buffers `disp_*` viven en el struct `OdApp` (heap).

---

#### `flipper_guard` — El Centinela
**¿Qué hace?** Monitor en tiempo real — pullea el CSV cada 1 segundo. Si creció,
parsea las líneas nuevas con overlork_events + overlork_rules y dispara LED/vibro
según la severidad.

**Arquitectura:**
```
FuriTimer (1s) ──► FGuardEventPoll ──► ViewDispatcher
                                           │
                              stat(file) ──┘
                              si creció:
                                ├─ fg_parse_new_events() ──► ol_rule_check() ──► ol_notify()
                                └─ fg_load_tail() ──► text_box_set_text()
```

**Parsing incremental:** `last_parsed_offset` guarda hasta dónde llegó el motor de
reglas. En el siguiente tick, solo lee desde ese offset en adelante → no re-parsea
el archivo completo cada segundo.

---

#### `rf_scanner` — El Radar
**¿Qué hace?** Barre 4 frecuencias ISM con el CC1101 en modo OOK, mide el RSSI
en cada canal 50 ms, y reporta señales sobre −90 dBm.

**Frecuencias:**
| Frecuencia | Región | Usos típicos |
|------------|--------|-------------|
| 315.000 MHz | Norte América | Mandos de garaje, autos |
| 433.920 MHz | Europa | ISM standard, sensores |
| 868.350 MHz | Europa | IoT, LoRa, alarmas |
| 915.000 MHz | USA | ISM, LoRa, medidores |

**Watch Mode (v0.1):**  
"Watch (5s)" — después de mostrar resultados, un `FuriTimerTypeOnce` de 5 segundos
dispara automáticamente un nuevo sweep. Ciclo infinito hasta que el usuario presione Back.
Útil para monitorear un ambiente RF mientras el Flipper está en el bolsillo (LED rojo = señal).

**Curiosidad del hardware:** El CC1101 tiene un tiempo de AGC (Automatic Gain Control)
de ~1–2 ms. Si lees el RSSI inmediatamente al entrar en RX, obtienes un valor falso.
Por eso el `furi_delay_ms(50)` — es el tiempo mínimo para que el AGC se estabilice.

---

#### `overlork_id` — La Firma Personal ✍️
**¿Qué hace?** Tarjeta de identidad digital — muestra los créditos del autor en
pantalla completa con estilo. App de un solo archivo, sin escenas, sin escena manager.

```
╔══════════════════════════════╗
║      OverL0rk  Suite        ║
╠══════════════════════════════╣
║  Author : Eudys Ramirez     ║
║  Handle : @OverL0rk         ║
║  Build  : Momentum FW 2026  ║
╠══════════════════════════════╣
║   6 libs · 5 FAPs · 6 hooks ║
╚══════════════════════════════╝
```

**Arquitectura mínima:** Solo usa `ViewPort` + `FuriMessageQueue`. Sin `SceneManager`,
sin `ViewDispatcher`. La app bloquea en `furi_message_queue_get(..., FuriWaitForever)`
hasta que el usuario presiona cualquier tecla. Es el patrón más pequeño posible para
una app interactiva en Flipper.

---

### 3.3 Hooks de Firmware (modificaciones al core)

| Archivo modificado | Subsistema | Evento registrado |
|-------------------|-----------|-------------------|
| `nfc/scenes/nfc_scene_read_success.c` | NFC | Tag leído con éxito |
| `subghz/scenes/subghz_scene_receiver_info.c` | SubGhz | Señal decodificada |
| `infrared/scenes/infrared_scene_remote.c` | IR | Señal IR transmitida |
| `bad_usb/scenes/bad_usb_scene_work.c` | BadKB | Script ejecutado |
| `lfrfid/scenes/lfrfid_scene_read_success.c` | LFRFID | Tarjeta LF leída |
| `ibutton/scenes/ibutton_scene_read_success.c` | iButton | Llave iButton leída |

**Patrón de hook:** `#include <audit/audit.h>` + `audit_log_event(...)` en el punto
exacto donde el subsistema ya tiene todos los datos parseados. Los hooks son
**completamente transparentes** — no cambian ningún comportamiento visible.

---

## 4. Cómo Funciona Cada Pieza

### El flujo completo de un evento

```
Usuario lee tarjeta NFC
         │
         ▼
nfc_scene_read_success.c
  audit_log_event("NFC", "READ", uid, proto)
         │
         ▼
lib/audit → /ext/audit/audit-2026-05-20.csv
  "2026-05-20 14:32:11,NFC,READ,04:AB:CD,NTAG215\n"
         │
         ├──────────────────────────────────────┐
         │                                      │
         ▼ (flipper_guard, tick 1s)            ▼ (overlork_dashboard, al abrir)
  ol_event_from_csv_line()             od_compute_stats()
         │                                      │
         ▼                              stats.nfc++
  ol_rule_check() → OlRuleActionWarn    stats.total++
         │                              indicator_text = "43"
         ▼
  ol_notify() → LED amarillo blink
```

### El flujo de rf_scanner watch mode

```
Usuario selecciona "Watch (5s)"
         │
         ▼
watch_mode = true → entra scan escena
         │
         ▼
rf_scanner_worker_start() → FuriThread
         │  [~200ms: 4 freqs × 50ms]
         ▼
RfScannerEventScanDone → resultado en TextBox
         │
         ▼
furi_timer_start(watch_timer, 5000ms)
         │  [5 segundos pasan]
         ▼
RfScannerEventWatchTick → "Scanning 4 freqs..."
         │
         ▼
rf_scanner_worker_start() → nuevo sweep...
         │  [Back presionado en cualquier momento]
         ▼
on_exit: furi_timer_stop() + rf_scanner_worker_stop()
```

---

## 5. Cosas Curiosas

### 🔬 Curiosidades técnicas

**1. El stack de 2 KB que casi mata pattern_analyzer**  
Los FAPs de Flipper tienen 2 KB de stack. `analyze_top_identifiers` necesitaba arrays
de 256 entradas × 8 bytes = 2 KB exactos. Pusimos el array en stack → crash
instantáneo. Solución: `malloc` para ese análisis específico.

**2. SubGhz se escribe con 'z' minúscula**  
El string en el firmware es `"SubGhz"` (no `"SubGHz"`). Los parsers del suite
aceptan ambas variantes para robustez, pero si escribes `"SubGHz"` en un hook,
el contador del dashboard no lo cuenta. Detalle de 1 carácter con consecuencias grandes.

**3. `widget_add_string_element` guarda un puntero, no una copia**  
La API de Widget en Flipper no copia el string — guarda el puntero. Si el buffer
fuente es una variable local en `on_enter()`, cuando `on_exit()` destruye el frame,
el Widget dibuja basura en pantalla. Por eso todos los `disp_*` del dashboard
viven en el heap del `OdApp`.

**4. `mbedtls_sha256_starts(&ctx, 0)` — el 0 importa**  
El segundo argumento es `is224`. Un `1` silencioso te da SHA-224 en lugar de SHA-256
sin ningún error ni warning. El digest tiene 28 bytes en vez de 32 y todo se rompe
de formas difíciles de debuggear.

**5. El AGC del CC1101 necesita 50ms para estabilizarse**  
Sin el `furi_delay_ms(50)`, el RSSI que lees en rx es el valor anterior del AGC,
no el de la frecuencia nueva. Todos los resultados del scanner serían incorrectos.

**6. `FuriThreadStateStopped` en threads nunca iniciados**  
Un `FuriThread` recién creado con `furi_thread_alloc_ex()` ya está en estado
`FuriThreadStateStopped`. Eso significa que `furi_thread_join()` en un thread que
nunca fue iniciado retorna inmediatamente — ningún deadlock. Usamos esto para
simplificar el restart en watch mode.

**7. El status-bar indicator es solo 14 píxeles de ancho**  
`GuiLayerStatusBarRight` tiene espacio limitado. Los ViewPorts en esa capa compiten
por espacio de derecha a izquierda. El ancho es crítico — si pides más del que hay,
los otros indicadores del sistema (BT, SD, batería) se solapan.

**8. Los hooks no consumen RAM**  
Todos los hooks son funciones en `.text` (Flash ROM), no en RAM. El único costo
de memoria en runtime es el archivo CSV en SD — que de todas formas ya estaba abierto
por el subsistema en el momento del hook.

### 🕵️ Curiosidades de seguridad

**9. LF RFID (EM4100, HID) = sin cifrado = clonación trivial**  
El 90% de los sistemas de control de acceso de oficinas/garajes más vendidos en
Latinoamérica usan LFRFID de 125 kHz sin ningún tipo de autenticación criptográfica.
El Flipper puede leer, guardar y emular estas tarjetas en segundos. El hook de LFRFID
documenta cada intento — si ves muchas lecturas seguidas en el log, algo sospechoso pasa.

**10. iButton / Dallas 1-Wire = protocolo de los 80s todavía en producción**  
iButton usa el protocolo Dallas 1-Wire de 1987. Muchos edificios en LatAm todavía
usan llaves DS1990A para control de acceso. No tienen ningún tipo de cifrado —
son básicamente UIDs de 64 bits que el Flipper puede leer, guardar y emular.

**11. SubGhz RX = la categoría "Alert" más probable en operación normal**  
En un ambiente urbano normal, el scanner capturará señales de autos, mandos de
garaje, sensores meteorológicos y alarmas todo el tiempo. El nivel "Alert" (rojo)
para SubGhz no significa "hay un atacante" — significa "hay actividad RF que merece
atención". El contexto lo da el operador, no el algoritmo.

---

## 6. Investigación Externa — Ideas de la Comunidad

La comunidad de Flipper Zero es enorme y activa. Aquí los proyectos más interesantes
encontrados en fuentes externas:

### 6.1 Apps de seguridad ofensiva/defensiva populares

| App | Descripción | Fuente |
|-----|-------------|--------|
| **ESP32 Marauder** | WiFi/BT pentesting desde el módulo dev board (Deauth, beacon spam, sniffing) | [GitHub](https://github.com/justcallmekoko/ESP32Marauder) |
| **Evil Portal** | Punto de acceso WiFi falso con página de login captive — captura credenciales | [GitHub](https://github.com/bigbrodude6119/flipper-zero-evil-portal) |
| **Spectrum Analyzer** | Visualizador de espectro RF en tiempo real (todos los canales CC1101) | [Lab.flipper.net](https://lab.flipper.net) |
| **Flipper Authenticator** | TOTP/HOTP (como Google Authenticator) nativo en el dispositivo | [GitHub](https://github.com/akopachov/flipper-zero_authenticator) |
| **Mouse Jiggler** | Simula movimiento de ratón vía BadUSB para evitar bloqueo de pantalla | Comunidad |
| **Sentry Safe Bypass** | Abre cajas fuertes electrónicas Sentry Safe sin PIN | Comunidad |
| **ProtoView** | Visualización y análisis detallado de señales SubGHz raw | [GitHub](https://github.com/antirez/protoview) |
| **Wardriver** | GPS + CC1101 → geo-tagged RF sweep, exporta a wigle.net | Momentum FW |
| **flipper-mcp** | Controla el Flipper vía Claude AI (MCP server sobre WiFi) | [GitHub](https://github.com/roostercoopllc/flipper-mcp) |
| **BLE Spam / BLE Killer** | Satura dispositivos Bluetooth cercanos con paquetes falsos | Comunidad |

### 6.2 Hardware expansions relevantes

| Hardware | Descripción |
|----------|-------------|
| **ESP32 Marauder 5G Apex 5** | 2× CC1101, ESP32-C5, nRF24, GPS — el módulo más completo al 2026 |
| **Feberis Pro** | Wardriving avanzado con geo-tagging automático |
| **Dev Board Pro GPS** | GPS ATGM336H + ESP32 para subdriving |
| **UHF RFID Board** | RFID experimental de UHF (860–960 MHz, largo alcance) |

### 6.3 Proyectos creativos/curiosos

| Proyecto | Por qué es curioso |
|----------|--------------------|
| **DOOM en Flipper** | El clásico corriendo en 64×128 px monocromo — 15fps |
| **TAMA P1 (Tamagotchi)** | Mascota virtual en el Flipper |
| **Tuning Fork** | Genera tonos de audio por GPIO — afinador de guitarra |
| **BPM Tapper** | Tap tempo para músicos (uso del botón OK) |
| **NFC Maker** | Crea tags NFC NDEF personalizados (URL, texto, WiFi QR) |
| **POCSAG Pager** | Lee mensajes de buscapersonas todavía activos en infraestructura de emergencias |
| **ClassicConverterWeb** | Convierte dumps de Mifare Classic de Proxmark al formato Flipper |

---

## 7. Ideas Implementables del Ecosistema

Basado en la investigación, estas son las mejoras más interesantes que podrían
añadirse al OverL0rk Suite:

### 🥇 Alta prioridad / Bajo esfuerzo

**A. TOTP Integrado en el Dashboard**  
Añadir una escena "TOTP" al overlork_dashboard que genere códigos de tiempo usando
`ol_crypto_hmac_sha256` (ya tenemos la función). El usuario configura la semilla
una vez en un archivo en la SD. Útil para 2FA offline en operaciones de campo.
> Esfuerzo: ~4 horas | Valor: muy alto

**B. Alerta de SubGHz en Replay Attack conocido**  
Extender el motor de reglas para detectar la firma de tiempo de un replay attack:
si el mismo patrón (duración y frecuencia) aparece >2 veces en <10 segundos,
es casi seguro un replay. Usando `overlork_protocol_id` para identificar el protocolo.
> Esfuerzo: ~3 horas | Valor: alto

**C. Exportar reporte a USB**  
Cuando el pattern_analyzer termina el análisis, ofrecer "Export via USB" que monta
el Flipper como Mass Storage y copia el report-*.txt al PC sin necesidad de sacar la SD.
> Esfuerzo: ~2 horas | Valor: buena UX

**D. Notificación sonora configurable**  
Añadir `ol_notify_sound()` que usa el GPIO de audio del Flipper para tocar un tono
corto en Critical. La vibración ya existe; el sonido añade una capa más.
> Esfuerzo: ~1 hora | Valor: medio

### 🥈 Media prioridad / Esfuerzo medio

**E. GPS SubDriving Log**  
Si hay un módulo GPS conectado al GPIO UART, añadir la posición GPS al registro de
auditoría. Cada evento tendría coordenadas → mapa de calor de actividad RF.
El formato CSV se extiende a 7 campos: añadir `lat,lon`.
> Esfuerzo: ~8 horas | Requiere: hardware GPS

**F. NFC Emulation Watcher**  
Hook en la escena de emulación NFC para registrar cuántas veces el Flipper fue
consultado por un lector (eventos de "interrogación"). Permite detectar si alguien
está intentando leer tu tarjeta emulada repetidamente.
> Esfuerzo: ~6 horas | Valor: alto para red team

**G. Pattern Analyzer en tiempo real**  
Integrar los algoritmos de pattern_analyzer en flipper_guard para detectar patrones
en vivo (no solo en análisis offline). Si se detecta `repeat_exposure` en live,
escalar la severidad automáticamente.
> Esfuerzo: ~10 horas | Valor: muy alto

### 🥉 Largo plazo / Alto esfuerzo

**H. Dashboard remoto via BLE**  
Usar el BLE del STM32WB55 para enviar estadísticas del dashboard a una app móvil.
El Flipper ya tiene BLE (lo usa para el remote control). Una app Android/iOS podría
mostrar el conteo de eventos en tiempo real.
> Esfuerzo: ~40 horas | Requiere: app móvil

**I. Cifrado del log de auditoría**  
Usar `overlork_crypto` para cifrar el CSV con AES-256-GCM antes de escribirlo.
Solo accesible con una clave almacenada en el secure element del STM32WB55.
> Esfuerzo: ~12 horas | Valor: máximo para entornos hostiles

---

## 8. Firma Personal — OverL0rk ID

Como marca personal de todo el trabajo realizado, se creó una aplicación dedicada
que funciona como **tarjeta de identidad digital**:

### `applications_user/overlork_id/` — El Sello Personal

```
╔══════════════════════════════╗
║      OverL0rk  Suite        ║  ← Nombre del suite
╠══════════════════════════════╣
║  Author : Eudys Ramirez     ║  ← Nombre real
║  Handle : @OverL0rk         ║  ← Handle / marca
║  Build  : Momentum FW 2026  ║  ← Contexto
╠══════════════════════════════╣
║   6 libs · 5 FAPs · 6 hooks ║  ← Stats del trabajo
╚══════════════════════════════╝
                     [ any key ]
```

Esta app aparece bajo el menú `OverL0rk` en el Flipper y sirve como:
- **Prueba de autoría** — "yo construí esto"
- **Resumen del trabajo** realizado en una sola pantalla
- **Easter egg** — quien encuentre la app entiende el alcance del proyecto

### La firma está también en:

- **overlork_dashboard About scene:** "OL Dashboard v0.2 / by Eudys Ramirez / @OverL0rk 2026"
- **rf_scanner About scene:** menciona al autor
- **Todos los headers de código:** `@brief ... by OverL0rk`
- **Ruta de apps:** `/ext/apps/OverL0rk/` — el nombre propio como namespace

### ¿Por qué `@OverL0rk`?

El nombre juega con `Overlord` (señor supremo) + `L0rk` (l33tspeak de `lork`/`lurk` = acechar,
monitorear en silencio). Es exactamente lo que hace el suite: **acechar en silencio todos los
eventos de RF y credenciales**, reportando sin interferir.

El `0` (cero) en lugar de la `O` es l33tspeak clásico — guiño al mundo del hacking ético
y la cultura hacker de los años 90.

---

## Apéndice — Inventario de archivos creados/modificados

### Nuevos (creados desde cero)

```
lib/audit/                              (extendido, no creado)
lib/overlork_events/
  ├── overlork_events.h
  └── overlork_events.c
lib/overlork_rules/
  ├── overlork_rules.h
  └── overlork_rules.c
lib/overlork_notify/
  ├── overlork_notify.h
  └── overlork_notify.c
lib/overlork_protocol_id/
  ├── overlork_protocol_id.h
  └── overlork_protocol_id.c
lib/overlork_crypto/
  ├── overlork_crypto.h
  └── overlork_crypto.c

applications_user/audit_viewer/
applications_user/pattern_analyzer/
applications_user/overlork_dashboard/
applications_user/flipper_guard/
applications_user/rf_scanner/
applications_user/overlork_id/          ← FIRMA PERSONAL
  ├── application.fam
  └── overlork_id.c

documentation/overlork_optimizations.md
documentation/overlork_informe_final.md ← ESTE DOCUMENTO
```

### Modificados (firmware core)

```
applications/main/nfc/scenes/nfc_scene_read_success.c
applications/main/subghz/scenes/subghz_scene_receiver_info.c
applications/main/infrared/scenes/infrared_scene_remote.c
applications/main/bad_usb/scenes/bad_usb_scene_work.c
applications/main/lfrfid/scenes/lfrfid_scene_read_success.c
applications/main/ibutton/scenes/ibutton_scene_read_success.c
lib/SConscript
```

---

*Informe generado el 26 de Mayo de 2026.*  
*Todo el código permanece local hasta autorización expresa del autor.*  
*🔒 @OverL0rk — Eudys Ramirez*
