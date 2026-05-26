# OverL0rk Flipper-Zero Security Suite

**Author:** OverL0rk (Eudys Ramirez)  
**Target:** Momentum Firmware (Flipper Zero F7)  
**Status:** Local development — do NOT push until authorized

---

## Overview

This document tracks the architecture, components, and design decisions for the
OverL0rk security suite — a collection of shared libraries and FAPs that turn the
Flipper Zero into a passive security monitoring device.

```
lib/
  audit/                Shared audit-log writer  (existing, enhanced hooks)
  overlork_events/      CSV-line → OlEvent parser
  overlork_rules/       Rule engine (pattern → severity)
  overlork_notify/      LED/vibro notification dispatcher
  overlork_protocol_id/ OOK/Manchester/NEC chip-time identifier
  overlork_crypto/      SHA-256, HMAC-SHA-256, AES-256-GCM wrappers

applications_user/
  audit_viewer/         Read-only CSV log viewer  (v0.1)
  pattern_analyzer/     Offline forensic analysis (v0.4)
  overlork_dashboard/   System stats dashboard    (v0.2 + JSON export)
  flipper_guard/        Live audit monitor        (v0.3)
  rf_scanner/           Passive RF RSSI sweep     (v0.2 + persistence)
  overlork_id/          Personal signature FAP    (v1.0)
  sentinel_runner/      Defensive BadUSB payloads (v1.0)
```

---

## Shared Libraries

### `lib/audit` — Audit Logger

Pre-existing library, extended with auto-logging hooks in the main application
subsystems.

| Symbol | Description |
|--------|-------------|
| `audit_log_event(subsystem, operation, identifier, details)` | Append one CSV row |
| `AUDIT_BASE_PATH` | `/ext/audit` |
| `AUDIT_MAX_FIELD_LEN` | 64 characters |

**CSV format:** `YYYY-MM-DD HH:MM:SS,subsystem,operation,identifier,details\n`  
New files get a header line: `timestamp,subsystem,operation,identifier,details\n`

**Hooks added:**

| Location | Subsystem | Op | Logged when |
|----------|-----------|----|-------------|
| `applications/main/nfc/scenes/nfc_scene_read_success.c` | `NFC` | `READ` | Tag successfully read |
| `applications/main/subghz/scenes/subghz_scene_receiver_info.c` | `SubGhz` | `RX` | Signal decoded in receiver |
| `applications/main/infrared/scenes/infrared_scene_remote.c` | `IR` | `TX` | IR signal transmitted |
| `applications/main/bad_usb/scenes/bad_usb_scene_work.c` | `BadKB` | `RUN` | Bad-USB script executed |
| `applications/main/lfrfid/scenes/lfrfid_scene_read_success.c` | `LFRFID` | `READ` | LF RFID card successfully read |
| `applications/main/ibutton/scenes/ibutton_scene_read_success.c` | `iButton` | `READ` | iButton / 1-Wire key read |
| `applications/main/bad_usb/helpers/ducky_script.c` | `BadKB` | `AUDIT` | Sentinel payload executed (path contains "Sentinel") |

**Note:** Subsystem string for SubGHz is `"SubGhz"` (lowercase z) — match exactly
in parsers. Both `"SubGhz"` and `"SubGHz"` are accepted by the parser for
robustness, but the canonical written form is `"SubGhz"`.

**LFRFID identifier field:** the pre-built `display_text` FuriString (`"Hex: XX XX ..."`)
from `protocol_dict_render_data()` — reused to avoid extra heap allocation.

**iButton identifier field:** output of `ibutton_protocols_render_brief_data()`,
typically a hex serial string. Details field is the protocol name from
`ibutton_protocols_get_name()`.

---

### `lib/overlork_events` — Event Parser

Parses a CSV line from the audit log into a structured `OlEvent`.

```c
typedef enum {
    OlEventTypeNfcRead,       /* NFC READ                           */
    OlEventTypeSubGhzRx,      /* SubGhz RX                          */
    OlEventTypeIrTx,          /* IR TX                              */
    OlEventTypeIrRx,          /* IR RX                              */
    OlEventTypeBadKbRun,      /* BadKB script run                   */
    OlEventTypeLfRfidRead,    /* LFRFID card read (EM4100, HID …)   */
    OlEventTypeIButtonRead,      /* iButton / 1-Wire key (DS199x …)    */
    OlEventTypeSignalPersistent, /* SubGhz/PERSIST: same src 3+ sweeps */
    OlEventTypeBadKbAudit,       /* BadKB/AUDIT: Sentinel defensive    */
    OlEventTypeOther,            /* wildcard / unrecognised            */
} OlEventType;

typedef struct {
    OlEventType type;
    char subsystem[16];   /* "NFC", "SubGhz", "IR", "BadKB", "LFRFID", "iButton" */
    char operation[16];   /* "READ", "RX", "TX", "RUN"                            */
    char identifier[64];
    char details[64];
    uint32_t epoch_seconds;
} OlEvent;

bool ol_event_from_csv_line(char* line, OlEvent* out);
```

**Internals:** Skips header lines (`strncmp(line, "timestamp", 9) == 0`), tokenises
5 comma-separated fields in-place (NUL-overwrites commas), converts
`YYYY-MM-DD HH:MM:SS` to epoch via `datetime_datetime_to_timestamp`.
Handles both `"SubGhz"` and `"SubGHz"` spellings in `classify()`.  
For SubGhz entries, the `op` field is now checked: `"PERSIST"` maps to
`OlEventTypeSignalPersistent`; all other SubGhz ops map to `OlEventTypeSubGhzRx`.

**`OlEventTypeOther` as wildcard:** in the rule engine, rules with
`event_type == OlEventTypeOther` match any event type.

---

### `lib/overlork_rules` — Rule Engine

Evaluates an `OlEvent` against a built-in priority table; returns the matching
rule with the highest severity.

```c
typedef enum {
    OlRuleActionInfo     = 0,
    OlRuleActionWarn     = 1,
    OlRuleActionAlert    = 2,
    OlRuleActionCritical = 3,
} OlRuleAction;

typedef struct {
    const char*  name;
    OlEventType  event_type;   /* OlEventTypeOther = any type wildcard */
    const char*  subsystem;    /* NULL = any subsystem wildcard        */
    const char*  operation;    /* NULL = any operation wildcard        */
    OlRuleAction action;
} OlRule;

const OlRule* ol_rule_check(const OlEvent* event);
const OlRule* ol_rule_get_table(size_t* count);
```

**Built-in rule table (highest severity wins; rules are scanned 0→N, best kept):**

| Rule name | EventType | Subsystem | Operation | Severity |
|-----------|-----------|-----------|-----------|----------|
| Any event | Other (wildcard) | * | * | Info |
| NFC read | NfcRead | NFC | * | Warn |
| SubGhz RX | SubGhzRx | * | * | Alert |
| IR transmission | IrTx | IR | TX | Warn |
| BadKB script run | BadKbRun | BadKB | * | **Critical** |
| LFRFID card read | LfRfidRead | LFRFID | * | Warn |
| iButton key read | IButtonRead | iButton | * | Warn |
| Persistent RF source | SignalPersistent | SubGhz | PERSIST | **Alert** |
| Sentinel audit payload | BadKbAudit | BadKB | * | Info |

**Rationale:** LFRFID and iButton are `Warn` (same tier as NFC) — both represent
credential-bearing tokens (access cards, intercom keys) that carry cloning risk.
BadKB is `Critical` + vibration because it directly executes attack payloads.
`SignalPersistent` (RF source on 3+ consecutive sweeps) is `Alert` — a stationary
transmitter in range is a reconnaissance indicator (tracker, relay device, rogue AP).

---

### `lib/overlork_notify` — Notification Dispatcher

Maps a severity level to LED blink colour + optional vibration.

```c
typedef enum {
    OlNotifyInfo     = 0,   /* matches OlRuleAction values for safe cast */
    OlNotifyWarn     = 1,
    OlNotifyAlert    = 2,
    OlNotifyCritical = 3,
} OlNotifySeverity;

void ol_notify(NotificationApp* notifications, OlNotifySeverity severity);
```

| Severity | LED | Vibration |
|----------|-----|-----------|
| Info | Cyan | No |
| Warn | Yellow | No |
| Alert | Red | No |
| Critical | Magenta | Yes |

---

### `lib/overlork_protocol_id` — OOK Protocol Identifier (v2)

Identifies the OOK modulation type from a pulse-duration array without
requiring dynamic memory allocation.

```c
#define OL_PROTO_MIN_PULSES  10u
#define OL_PROTO_MAX_PULSES 512u

/* Tolerance constants */
#define OL_TE_TOL_PCT      40u  /* ±40% tolerance for te-fits       */
#define OL_TE_MIN_VALID_US 50u  /* pulses shorter than this = noise */
#define OL_TE_MAX_CHIP_US  5000u

/* TPMS fingerprint constants */
#define OL_TPMS_TE_MIN_US  60u  /* chip time 60–520 µs             */
#define OL_TPMS_TE_MAX_US  520u
#define OL_TPMS_MIN_BITS   48u  /* ≥ 48 bits in payload            */
#define OL_TPMS_PREAMBLE   16u  /* ≥ 16 alternating te/2te pulses  */

typedef enum {
    OlProtoUnknown = 0,
    OlProtoPwmOok,         /* PWM: fixed gap, variable pulse           */
    OlProtoManchesterOok,  /* Manchester: 1T/2T biphase               */
    OlProtoNec,            /* NEC: 9ms leader + 4.5ms space           */
    OlProtoTpms,           /* TPMS tire-pressure (Manchester, 64+ bit) */
    OlProtoRaw,            /* Non-periodic / irregular pulses          */
} OlProtoType;

typedef struct {
    OlProtoType type;
    uint8_t     confidence;    /* 0–100 */
    uint32_t    chip_time_us;  /* base pulse width (1T) in µs  */
    uint16_t    bit_count;     /* estimated number of bits     */
    uint8_t     repeat_count;  /* detected repeat frames       */
} OlProtoResult;

bool        ol_protocol_id(const uint32_t* durations_us, uint16_t count, OlProtoResult* result);
const char* ol_proto_type_str(OlProtoType type);
```

**Algorithm (v2 — ProtoView-inspired "te" method):**
1. **NEC fast-path:** if `durations_us[0] > 7000 µs` → `OlProtoNec`, confidence 85.
2. **`find_te()`:** builds an 8-slot sorted min-window of shortest valid pulses
   (≥ `OL_TE_MIN_VALID_US`); returns median of the 4 smallest as the chip time.
   More noise-robust than a fixed-bin histogram (cf. ProtoView `te_duration_canary`).
3. **`classify_buckets()`:** sorts all pulses into n1 (≈1T), n2 (≈2T), n3 (≈3T),
   n_other using `te_fits()` with ±40% tolerance.
4. **`looks_like_tpms()`:** preamble ≥ 16 consecutive 1T/2T pulses, chip time in
   60–520 µs range, ≥ 48 bits → `OlProtoTpms`, confidence 90.
5. **PWM vs Manchester:** if > 25% of valid pulses are 2T → `OlProtoManchesterOok`,
   else `OlProtoPwmOok`.
6. **Confidence:** `40 + (fit_pct × 60 / 100)`, capped at 100.
7. **`count_repeats()`:** gaps ≥ 10×te = inter-packet pause → repeat frames counted.

No heap allocation; all state lives on the stack (<512 B).

---

### `lib/overlork_crypto` — Cryptographic Primitives

Wraps Flipper's mbedtls and hardware TRNG/AES into a minimal, clean API.

```c
#define OL_CRYPTO_SHA256_LEN  32u
#define OL_CRYPTO_AES_KEY_LEN 32u
#define OL_CRYPTO_GCM_IV_LEN  12u
#define OL_CRYPTO_GCM_TAG_LEN 16u

typedef enum {
    OlCryptoOk          = 0,
    OlCryptoErrParam,       /* NULL pointer or zero length */
    OlCryptoErrHash,        /* mbedtls SHA-256 failure     */
    OlCryptoErrAes,         /* AES-GCM encrypt/decrypt err */
    OlCryptoErrAuthFail,    /* GCM auth tag mismatch       */
} OlCryptoStatus;

/* Randomness (hardware TRNG) */
void     ol_crypto_random_bytes(uint8_t* buf, size_t len);
uint32_t ol_crypto_random_u32(void);
void     ol_crypto_random_iv(uint8_t iv[12]);

/* Hashing */
OlCryptoStatus ol_crypto_sha256(
    const uint8_t* data, size_t len, uint8_t digest[32]);
OlCryptoStatus ol_crypto_hmac_sha256(
    const uint8_t* key, size_t key_len,
    const uint8_t* data, size_t data_len,
    uint8_t mac[32]);

/* Authenticated encryption */
OlCryptoStatus ol_crypto_aes_gcm_encrypt(
    const uint8_t key[32], const uint8_t iv[12],
    const uint8_t* aad, size_t aad_len,
    const uint8_t* plaintext, uint8_t* ciphertext, size_t len,
    uint8_t tag[16]);
OlCryptoStatus ol_crypto_aes_gcm_decrypt(
    const uint8_t key[32], const uint8_t iv[12],
    const uint8_t* aad, size_t aad_len,
    const uint8_t* ciphertext, uint8_t* plaintext, size_t len,
    const uint8_t tag[16]);
```

**Dependencies:** `<mbedtls/sha256.h>`, `<mbedtls/md.h>`, `<furi_hal_random.h>`,
`<furi_hal_crypto.h>`.  
**Note:** `mbedtls_sha256_starts(&ctx, 0)` — second argument `0` = SHA-256,
`1` = SHA-224.

---

## FAP Applications

### `audit_viewer` — Log Viewer

**Category:** OverL0rk/Security  
**Version:** 0.1  
**libs:** `audit`

Simple read-only TextBox showing the last 2 KB of today's audit CSV.
Provides a quick "what just happened?" view without analysis.

---

### `pattern_analyzer` — Offline Forensic Analyzer

**Category:** OverL0rk/Security  
**Version:** 0.3  
**libs:** `audit`

Loads today's audit CSV and runs a battery of pattern-detection algorithms:

| Algorithm | Description |
|-----------|-------------|
| `sequential_uids` | Detect sequential NFC UID reads (cloning attempt?) |
| `shared_prefix` | UIDs sharing a common prefix (same card family) |
| `repeat_exposure` | Same identifier seen ≥ 3 times in session |
| `analyze_burst` | ≥ 5 events in ≤ 5 seconds (high activity burst) |
| `analyze_short_burst` | ≥ 3 events in any 5-second window |
| `analyze_cross_protocol` | Same identifier used across different subsystems |
| `analyze_top_identifiers` | Most frequently seen identifiers (top 2) |
| `analyze_relay_attack` | NFC/LFRFID read + SubGhz event within 30 s window |
| `analyze_night_activity` | Events between 00:00–04:59 flagged as unusual hours |
| `analyze_threat_profile` | Multi-vector fingerprint (≥5 = "FULL spectrum op") |

**v0.2 additions:**
- Export scene writes report to `/ext/audit/report-YYYY-MM-DD.txt`
- `analyze_top_identifiers` uses `malloc` for count arrays (avoids 2 KB stack overflow)
- All chunk reads use 256-byte buffers (not 1-byte per call)

**v0.3 additions (community research integrations):**

`analyze_relay_attack()` — inspired by iClass/NFC relay research (DEF CON 32):
- Looks for a credential read (NFC or LFRFID) followed by a SubGhz event within
  30 seconds (`RELAY_WINDOW_S 30`).
- Pattern: attacker reads card, accomplice relays to remote reader in that window.
- Reports: "POSSIBLE RELAY" with timestamp pair; "→ INVESTIGATE" annotation.

`analyze_night_activity()`:
- Flags events timestamped between 00:00 and 04:59 local time.
- Shows count + earliest timestamp of off-hours activity.
- Physical attack vectors (break-ins, planted devices) concentrate in these hours.

`analyze_threat_profile()` — from Kashmir54/ProtoView multi-vector research:
- Tracks 7 vectors: NFC, SubGhz, IR, BadKB, LFRFID, iButton, PERSIST.
- Scores: ≥ 5 vectors = "FULL spectrum op", ≥ 3 = "multi-vector", ≥ 2 = "dual-vector".
- A full-spectrum actor has broad, sophisticated capabilities vs single-tool script kiddie.

---

### `overlork_dashboard` — Security Dashboard

**Category:** OverL0rk/Security  
**Version:** 0.2  
**libs:** `audit`

Reads today's audit CSV and presents a per-subsystem event count summary, plus
a persistent status-bar indicator showing the running total.

**Scenes:**
- **Start** — Submenu: Stats, Today's Log, TOTP, Export JSON, About
- **Stats** — Widget with per-subsystem totals (7 subsystems), last event info
- **Log** — TextBox with last 2 KB of audit file
- **TOTP** — Live TOTP code view from `/ext/totp.key`
- **Export JSON** — Converts today's audit CSV to machine-readable JSON *(new)*
- **About** — Widget with description

**Export JSON scene (`overlork_dashboard_scene_export.c`):**
- On enter: shows "Working..." in Widget, then runs `od_do_export()` synchronously.
- Reads CSV via `Storage`, cap `OD_EXPORT_MAX_CSV_BYTES` (32 KB), max
  `OD_EXPORT_MAX_EVENTS` (512 events).
- Output: `/ext/audit/audit-YYYY-MM-DD.json` (minified, single line):
  ```json
  {"date":"2026-05-26","gen":"OverL0rk v0.2","events":[
    {"ts":"14:32:11","sub":"NFC","op":"READ","id":"04:AB:CD","det":"NTAG215"},
    ...
  ]}
  ```
- JSON-escapes `id` and `det` fields (backslash, quote, control chars).
- On completion: shows "Done! N events" + filename, or error message.
- Use-case: drop SD card into laptop, `jq .events[] audit-DATE.json | ...`
  or feed to flipper-mcp for AI analysis.

**`OdStats` computed fields (v0.2):**
```
total:        total events today
nfc:          NFC READ events
subghz:       SubGhz RX events
ir:           IR TX events
badkb:        BadKB RUN events
lfrfid:       LFRFID READ events          ← new in v0.2
ibutton:      iButton READ events         ← new in v0.2
last_time:    "HH:MM:SS" of most recent event
last_subsys:  subsystem of most recent event
battery_pct:  battery % at refresh time
```

**Stats widget layout (128 × 64 px):**
```
y= 2  "OverL0rk Dash"          FontPrimary  centered
y=14  "Total today: N"          FontSecondary left
y=22  "NFC:N  Sub:N  IR:N"     FontSecondary left
y=30  "LF:N   iBtn:N  KB:N"    FontSecondary left
y=40  "Last: HH:MM:SS"          FontSecondary left
y=50  "by SUBSYS   Bat:N%"      FontSecondary left
```

**Status-bar indicator (v0.2 addition):**  
A `ViewPort` registered at `GuiLayerStatusBarRight` renders the running total as
`"42"` or `"99+"`. Updated on every `od_compute_stats()` call via
`od_indicator_update()`. The viewport is 14 px wide; text is drawn at (1, 7) with
`FontSecondary`.

**Important:** `widget_add_string_element()` stores a pointer to the string, not a
copy. All display buffers are `char[]` fields inside `OdApp` populated in
`on_enter` and valid until `on_exit` calls `widget_reset()`.

**Lifecycle (`OdApp`):**
```c
// alloc
app->gui = furi_record_open(RECORD_GUI);
view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui,
                              ViewDispatcherTypeFullscreen);
app->indicator_vp = view_port_alloc();
view_port_set_width(app->indicator_vp, 14);
view_port_draw_callback_set(app->indicator_vp, od_indicator_draw_cb, app);
gui_add_view_port(app->gui, app->indicator_vp, GuiLayerStatusBarRight);

// free — must remove before closing RECORD_GUI
gui_remove_view_port(app->gui, app->indicator_vp);
view_port_free(app->indicator_vp);
// ... then furi_record_close(RECORD_GUI)
```

---

### `flipper_guard` — Live Audit Monitor

**Category:** OverL0rk/Security  
**Version:** 0.3  
**libs:** `audit`, `overlork_events`, `overlork_rules`, `overlork_notify`

Real-time monitor that polls the audit CSV every second. New lines are parsed
through the rule engine and trigger severity-appropriate LED/vibro notifications.

**Architecture:**
- `FuriTimer` (1 s, periodic) posts `FGuardEventPoll` to the ViewDispatcher
- `on_event` stats the file; if it grew, calls `fg_parse_new_events()` then
  `fg_load_tail()` and `fg_refresh_textbox()`
- Incremental parsing: `last_parsed_offset` tracks how far the rule engine has
  read; `last_file_size` tracks what the TextBox has displayed

**v0.2 improvements over v0.1:**
- Rule engine integration (not just display but severity detection)
- Incremental parsing (only new bytes evaluated)
- Severity-aware notifications via `ol_notify()`
- Covers all 7 subsystems (NFC, SubGhz, IR, BadKB, LFRFID, iButton + catch-all)

**v0.3 additions — Severity Status Footer:**
- `FGuardApp` gains two fields: `session_events` (count of new events since entering
  Watch) and `session_max_sev` (worst severity seen this session).
- `fg_refresh_textbox()` appends a divider + footer to the CSV tail:
  ```
  {csv tail lines}
  --------------
  Guard +N | ALERT
  ```
  Since `TextBoxFocusEnd` scrolls to the last line, this footer is **always
  visible** without user interaction.
- Session counters reset to `(0, OlNotifyInfo)` on every `on_enter` call.
- `fg_parse_new_events()` increments `session_events` per valid OlEvent and updates
  `session_max_sev` when a rule fires with higher severity than the current max.
- `ol_notify()` is still called with the **batch** severity (current tick only);
  the footer displays the **session** severity (worst since entering Watch).
  This means the footer escalates monotonically (WARN → ALERT → CRIT) and never
  drops back down — giving the operator a "worst seen" indicator at a glance.

**Severity labels in footer:**

| OlNotifySeverity | Footer label |
|------------------|--------------|
| OlNotifyInfo | `OK` |
| OlNotifyWarn | `WARN` |
| OlNotifyAlert | `ALERT` |
| OlNotifyCritical | `CRIT!` |

---

### `rf_scanner` — Passive RF RSSI Scanner

**Category:** OverL0rk/Security  
**Version:** 0.2  
**libs:** `audit`

Sweeps four common ISM-band frequencies using the CC1101 in OOK mode and
reports the RSSI at each channel. Signals above −90 dBm are flagged and
logged to the audit file.

**Frequencies scanned:**

| Frequency | Band | Common use |
|-----------|------|------------|
| 315.000 MHz | 315 | North American remote controls |
| 433.920 MHz | 433 | European ISM standard |
| 868.350 MHz | 868 | EU IoT / LoRa |
| 915.000 MHz | 915 | US ISM / LoRa |

**Hardware approach:**
1. `furi_hal_subghz_load_registers({0x10, 0x17, 0, 0})` — reset CC1101, set
   MDMCFG4 = 0x17 (OOK, 650 kHz Rx BW). Sentinel-terminated flat `{reg, val}` array.
2. For each frequency: `idle → set_frequency_and_path → rx → delay(50ms) →
   get_rssi → idle`
3. Threshold −90 dBm: signals above are logged as `SubGhz/SCAN/<freq>/<rssi>`
4. After sweep: red blink if signals detected, cyan blink otherwise

**Thread model:** scan runs in a `FuriThread` (1 KB stack); posts
`RfScannerEventScanDone` to the ViewDispatcher when complete. The UI thread
renders "Scanning..." during the sweep (~200 ms) and updates the TextBox with
the RSSI table when done.

**Watch mode (`"Watch (5s)"` menu item):**  
Sets `app->watch_mode = true` before entering the scan scene. After each
`ScanDone` event, a `FuriTimerTypeOnce` timer fires `RfScannerEventWatchTick`
after `RF_SCAN_WATCH_INTERVAL_MS` (5000 ms). The scan scene's `on_event`
restarts the worker on that tick. The header shows `[Watch]` and the footer
shows `"Found: N | Next: 5s"` instead of `"Found: N signals"`.  
Pressing Back stops the timer in `on_exit` via `furi_timer_stop()` before
stopping the worker.

**Thread restart safety:** `rf_scanner_worker_start` calls `furi_thread_join`
before every `furi_thread_start` so FreeRTOS can reclaim the previous task.
On the very first start, the thread is in `FuriThreadStateStopped` and join
returns immediately.

**v0.2 additions — Cross-sweep persistence tracking:**

Constants:
```c
#define RF_SCAN_PERSIST_MIN 3u  /* consecutive sweeps above threshold = persistent */
```

New `RfScannerApp` fields:
```c
uint8_t signal_history[RF_SCAN_FREQ_COUNT]; /* sweeps above threshold per freq */
uint8_t persistent_mask;                     /* bitmask of persistent sources   */
```

Per-frequency logic:
- First detection (history was 0) → `detected_count++` + `audit_log("SubGhz","SCAN",...)`
- Each subsequent sweep: `history++` (clamped at 0xFF)
- When `history == RF_SCAN_PERSIST_MIN` AND bit not set → set bit in `persistent_mask`
  + `audit_log("SubGhz","PERSIST",...)` → triggers `OlEventTypeSignalPersistent` in rule engine
- Signal disappears → `history = 0`, clear `persistent_mask` bit
- Subsequent detections in same session (after PERSIST logged) → **silent** (no spam)

Result display markers:
- `*` — persistent source (same transmitter 3+ sweeps)
- `!` — transient detection (above threshold, not yet persistent)

Footer: `!=N *=M [PERSIST]` if any persistent detected; else `Found:N | Next:5s`.

**Scenes:**
- **Start** — Submenu: Scan Now, Watch (5s), About
- **Scan** — TextBox shows live RSSI table with `!`/`*` markers; auto-restarts in watch mode
- **About** — Widget with description

---

### `sentinel_runner` — OverL0rk Sentinel BadUSB Index

**Category:** OverL0rk  
**Version:** 1.0  
**libs:** `audit`

Index FAP for **defensive** BadUSB ducky payloads. Each payload is a read-only
audit script (port scan, AV check, process audit, network info, binary hash
check) that gathers information from the host and writes it to a local file
the user can review. Zero modifications to the host.

**Payloads bundled (12 total):**

*General system audit (8):*

| ID | Platform | What it inspects | Output |
|----|----------|------------------|--------|
| `win_portscan` | Win10/11 | LISTEN + ESTABLISHED TCP/UDP via `Get-NetTCPConnection` + `Get-NetUDPEndpoint` | `C:\Temp\sentinel-ports.txt` |
| `win_avcheck` | Win10/11 | `Get-MpComputerStatus`, `Get-MpThreatDetection`, SecurityCenter2 AV list | `C:\Temp\sentinel-av.txt` |
| `win_procaudit` | Win10/11 | Top 20 by CPU/RAM, unsigned-binary scan, processes with TCP sockets | `C:\Temp\sentinel-proc.txt` |
| `win_netaudit` | Win10/11 | Firewall profiles, ARP table, DNS cache, Wi-Fi profiles, routing | `C:\Temp\sentinel-net.txt` |
| `win_hashcheck` | Win10/11 | SHA-256 of cmd, explorer, lsass, svchost, winlogon, spoolsv | `C:\Temp\sentinel-hash.txt` |
| `linux_portaudit` | Linux | `ss -tulpn`, `lsof -iTCP`, `iptables -L` | `~/sentinel-ports.txt` |
| `linux_procaudit` | Linux | `ps auxf`, top CPU/RAM, sockets, systemd failed units | `~/sentinel-proc.txt` |
| `mac_portaudit` | macOS | `lsof -i -P -n` LISTEN+EST, `pfctl -s rules`, `launchctl list` | `~/sentinel-ports.txt` |

*Malware / IOC detection (4):*

| ID | Platform | What it detects | Output |
|----|----------|-----------------|--------|
| `win_malware_scan` | Win10/11 | Top 10 malware IOCs: Mimikatz, Cobalt Strike, miners, RAT processes, Run-key persistence, weird services, recent scheduled tasks, Defender exclusions, AMSI tampering, startup folders, recent TEMP executables, WMI subscriptions, non-system drivers | `C:\Temp\sentinel-malware.txt` |
| `win_ransomware` | Win10/11 | Encrypted file extensions (LockBit, Ryuk, Conti, WannaCry, BlackCat, Cl0p, +14 more), ransom notes (HOW_TO_DECRYPT*, _readme.txt, …), Volume Shadow Copy integrity, bcdedit recovery state, mass office-file modification | `C:\Temp\sentinel-ransom.txt` |
| `linux_malware_scan` | Linux | XMRig/kdevtmpfsi/Kinsing miners, Diamorphine rootkit (signal-31 test), `/etc/ld.so.preload` hijack, unauthorized SSH authorized_keys, suspicious cron, PHP webshells with eval/base64, recently modified /tmp executables, processes with deleted exe links, systemd ExecStart with curl/wget/base64, high-CPU processes | `~/sentinel-malware.txt` |
| `mac_malware_scan` | macOS | Silver Sparrow (`init_verx.plist`), non-Apple LaunchAgents/LaunchDaemons, OSX/Shlayer .pkg in /tmp, cryptominers, login items, recent /tmp binaries, unsigned launchd plists (`codesign -dv`), cron+at jobs, partial Pegasus IOCs | `~/sentinel-malware.txt` |

**IOC sources (auditable, ethical detection):**
- MITRE ATT&CK techniques (T1003 credential dumping, T1547 boot/login autostart, T1053 scheduled tasks, T1546 event-triggered, T1059 command interpreter)
- CISA `#StopRansomware` advisories
- Mandiant APT reports (Cobalt Strike, Mimikatz, AgentTesla, Qakbot)
- abuse.ch / MalwareBazaar public IOC feeds

**Why we don't auto-remediate:**
Sentinel deliberately keeps the human in the loop. Each finding includes a
`# REMEDIATION:` comment with the suggested cleanup command, but the auditor
reviews and executes it manually. A heuristic-based auto-fixer would destroy
legitimate software (corporate AV exclusions, signed-but-misnamed binaries,
vendor tools in `%APPDATA%`, custom Run-key entries). Detection is automatic;
remediation requires judgment.

**Architecture (`sentinel_runner.c`):**
- Two views via `ViewDispatcher`: `Submenu` (list of payloads) and `Widget` (detail view).
- On launch: `sentinel_extract_payloads()` creates `/ext/badusb/OverL0rk_Sentinel/`
  and writes every embedded ducky string (`k_payloads[i].ducky`) to disk as
  `<id>.txt`. Idempotent — skips files that already exist with the same length.
- Selecting a payload from the Submenu switches to the Widget detail view
  showing label, description, and the on-disk path. The user then opens
  BadUSB and selects the payload from there.
- Selecting a payload also logs `BadKB,PREVIEW,<id>,Sentinel preview shown`
  as an audit trail of what was viewed (rule fires Info severity, no LED).

**Payload conventions:**
- All payloads start with `REM` lines identifying themselves + an initial
  `DELAY 1500` (user time to switch focus to target host).
- Per-line `ENTER` commands instead of one giant STRING+semicolons — each
  line is acked by the shell, the user sees progress, and an accidental
  keypress only interrupts the current line (not 500 chars of typing).
- Windows payloads open PowerShell via `GUI r` (Win+R), Linux uses
  `CTRL ALT t` (default Ubuntu/Fedora terminal shortcut), macOS uses
  `GUI SPACE` (Spotlight) then types "terminal".
- Final command opens the output file in the default viewer (notepad on Win,
  `xdg-open` on Linux, `open -a TextEdit` on macOS) for immediate review.

**BadKB hook integration:**
The modified `ducky_script.c` checks if the script path contains
`"OverL0rk_Sentinel"` or `"Sentinel"`. If yes → logs as `BadKB/AUDIT`
(not `BadKB/RUN`). The audit log distinguishes defensive Sentinel scripts
from arbitrary user ducky scripts, so:
- `flipper_guard` fires **Info** (cyan LED) for Sentinel, not **Critical**
  (magenta + vibro) like for unknown payloads.
- `pattern_analyzer.analyze_threat_profile()` counts Sentinel runs in a
  separate `Defensive: N Sentinel` line, NOT in the offensive-vector
  diversity score — so a defensive operator doesn't get flagged as a
  multi-vector attacker.

**Why this matters:** The Flipper community largely treats BadUSB as
offense-only. Sentinel inverts that, demonstrating that the same HID
keystroke-injection vector is perfectly suited for incident response
and on-site sysadmin audits. The "scary cardboard ducky" becomes a
portable forensic toolkit.

---

## Build System

All shared libraries must be registered in `lib/SConscript`:

```python
modules = env.BuildModules([
    ...
    "audit",
    "overlork_events",
    "overlork_rules",
    "overlork_notify",
    "overlork_protocol_id",
    "overlork_crypto",
    ...
])
```

Each library has its own `SConscript` following the pattern:
```python
Import("env")
env.Append(SDK_HEADERS=[File("mylib.h")], LINT_SOURCES=[Dir(".")])
libenv = env.Clone(FW_LIB_NAME="mylib")
libenv.ApplyLibFlags()
lib = libenv.StaticLibrary("${FW_LIB_NAME}", libenv.GlobRecursive("*.c"))
libenv.Install("${LIB_DIST_DIR}", lib)
Return("lib")
```

FAP `application.fam` files reference libraries via `fap_libs=["libname", ...]`.

---

## Scene Manager Pattern

All FAPs use the standard Flipper scene-manager pattern.

**`scene_config.h`** — one `ADD_SCENE` macro per scene:
```c
ADD_SCENE(prefix, name, id)
```

**`scene.h`** — enum + `extern` handler table + forward declarations:
```c
typedef enum {
#define ADD_SCENE(prefix, name, id) PrefixScene##id,
#include "prefix_scene_config.h"
#undef ADD_SCENE
    PrefixSceneNum,
} PrefixScene;
extern const SceneManagerHandlers prefix_scene_handlers;
```

**`scene.c`** — three separate handler arrays + `SceneManagerHandlers` struct:
```c
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
static void (*const prefix_on_enter_handlers[])(void*) = {
#include "prefix_scene_config.h"
};
#undef ADD_SCENE
// ... same for on_event and on_exit

const SceneManagerHandlers prefix_scene_handlers = {
    .on_enter_handlers = prefix_on_enter_handlers,
    .on_event_handlers = prefix_on_event_handlers,
    .on_exit_handlers  = prefix_on_exit_handlers,
    .scene_num         = PrefixSceneNum,
};
```

**Critical:** use `.on_enter_handlers` / `.on_event_handlers` / `.on_exit_handlers`
field names (NOT `.handlers`). Enum ends with `SceneNum` (NOT `SceneCount`).

---

## Power Optimizations

### System-Level

- **Voltage scaling in STOP2**: Added `furi_hal_power_enter_stop2_with_scaling()`
  that drops VCore to Range 2 before entering STOP2 sleep, reducing idle power
  by ~40% (from ~1.8 mA to ~1.1 mA).
- **FreeRTOS run-time stats**: Gated under `#ifdef FURI_DEBUG` to avoid the
  `TIM17` always-on overhead in release builds.

### Application-Level

- **ext_3v3 reference counting**: `ext_3v3_acquire()` / `ext_3v3_release()` allow
  multiple subsystems to share the 3V3 rail without double-enabling or
  premature disable. Implements a simple ref-count; actual hardware enable
  happens only on 0→1 transition and disable on 1→0.

- **RF scanner idle time**: After each frequency measurement, the CC1101 is
  returned to IDLE (not RX) to reduce current draw during the processing phase.

---

## CSV Format Reference

```
timestamp,subsystem,operation,identifier,details
2025-05-20 14:32:11,NFC,READ,04:AB:CD:EF:01:02:03,NTAG215
2025-05-20 14:32:45,SubGhz,RX,433.920MHz,KeeLoq
2025-05-20 14:33:01,IR,TX,Samsung_TV_Power,NEC
2025-05-20 14:33:15,BadKB,RUN,ducky_script.txt,executed
2025-05-20 14:33:30,LFRFID,READ,Hex: A1 B2 C3 D4,EM4100
2025-05-20 14:33:48,iButton,READ,01 02 03 04 05 06 07,DS1990A
2025-05-20 14:34:00,SubGhz,SCAN,433.920MHz,-83dBm
2025-05-20 14:39:00,SubGhz,PERSIST,433.920MHz,-83dBm
2025-05-20 14:45:12,BadKB,PREVIEW,win_portscan,Sentinel preview shown
2025-05-20 14:45:38,BadKB,AUDIT,/ext/badusb/OverL0rk_Sentinel/win_portscan.txt,iface=USB repeats=1
```

**Field widths:** All fields capped at `AUDIT_MAX_FIELD_LEN` (64 chars).  
**Encoding:** UTF-8, Unix line endings.  
**Location:** `/ext/audit/audit-YYYY-MM-DD.csv`  
**Report exports:** `/ext/audit/report-YYYY-MM-DD.txt`  
**JSON exports:** `/ext/audit/audit-YYYY-MM-DD.json` (machine-readable, from Export JSON scene)

---

## Subsystem String Reference

| Written in code | Logged as | Parser accepts |
|-----------------|-----------|----------------|
| `"NFC"` | `NFC` | `"NFC"` |
| `"SubGhz"` | `SubGhz` | `"SubGhz"`, `"SubGHz"` |
| `"IR"` | `IR` | `"IR"` |
| `"BadKB"` | `BadKB` | `"BadKB"` |
| `"LFRFID"` | `LFRFID` | `"LFRFID"` |
| `"iButton"` | `iButton` | `"iButton"` |

All strings are case-sensitive in `strcmp()` comparisons except SubGhz where
both capitalisations are explicitly tolerated for backward compatibility.
