/**
 * @file rf_scanner_worker.c
 * @brief Passive RF frequency scanner worker thread — v2 with persistence tracking.
 *
 * v2 improvements:
 * ─────────────────
 * Cross-sweep persistence detection (inspired by TPMS/ProtoView research):
 *   - A transient signal (car driving by) is detected in 1-2 sweeps → logged SCAN.
 *   - A persistent source (IoT sensor, tracker, repeater) stays above the
 *     RSSI threshold across RF_SCAN_PERSIST_MIN consecutive sweeps → logged PERSIST.
 *   - PERSIST events flow through overlork_events/rules as OlEventTypeSignalPersistent
 *     → OlRuleActionAlert, triggering a red LED notification in flipper_guard.
 *
 * Logging strategy (reduces noise vs v1):
 *   - First detection in a new session → log SubGhz/SCAN (transient).
 *   - 3rd consecutive detection → log SubGhz/PERSIST (stationary source).
 *   - Subsequent detections in same session → silent (no audit spam).
 *   - Signal loss → reset history and persistent_mask for that frequency.
 *
 * Author : Eudys Ramirez (@OverL0rk)
 */

#include "rf_scanner_i.h"

/* ── CC1101 preset ─────────────────────────────────────────────────────── */
/*
 * CC1101 MDMCFG4 (reg 0x10) = 0x17 → OOK, Rx BW 650 kHz.
 * furi_hal_subghz_load_registers() takes a sentinel-terminated {reg, val} array.
 */
static const uint8_t rf_preset_ook_650khz[] = {
    0x10, 0x17, /* MDMCFG4: Rx BW 650 kHz OOK */
    0x00, 0x00, /* end sentinel                */
};

/* ── Scan frequency table ──────────────────────────────────────────────── */

static const uint32_t rf_scan_freqs[RF_SCAN_FREQ_COUNT] = {
    315000000u, /* 315.000 MHz — North-American remote controls / TPMS  */
    433920000u, /* 433.920 MHz — European ISM / TPMS / KeeLoq           */
    868350000u, /* 868.350 MHz — EU IoT / LoRa / alarm sensors          */
    915000000u, /* 915.000 MHz — US ISM / LoRa                          */
};

/* ── Worker thread ─────────────────────────────────────────────────────── */

int32_t rf_scanner_worker_thread(void* context) {
    RfScannerApp* app = context;

    /* Reset per-sweep counter only — history/persistent_mask persist across
     * watch-mode sweeps and must NOT be reset here. */
    app->detected_count = 0;

    /* Load OOK 650 kHz preset; performs a CC1101 reset internally */
    furi_hal_subghz_load_registers(rf_preset_ook_650khz);

    for(uint8_t i = 0; i < RF_SCAN_FREQ_COUNT && !app->scan_stop; i++) {
        uint32_t freq = rf_scan_freqs[i];

        app->results[i].freq_hz  = freq;
        app->results[i].rssi     = -127.0f;
        app->results[i].detected = false;

        if(!furi_hal_subghz_is_frequency_valid(freq)) {
            /* Frequency outside CC1101 range; leave at -127 dBm */
            app->signal_history[i] = 0;
            app->persistent_mask  &= ~(1u << i);
            continue;
        }

        /* Switch to idle → configure path → enter Rx → sample RSSI.
         * The 50ms dwell lets the CC1101 AGC stabilise (AGC settle ≈ 1–2ms,
         * but 50ms gives margin for slow signal envelopes). */
        furi_hal_subghz_idle();
        furi_hal_subghz_set_frequency_and_path(freq);
        furi_hal_subghz_rx();
        furi_delay_ms(50);
        float rssi = furi_hal_subghz_get_rssi();
        furi_hal_subghz_idle();

        app->results[i].rssi     = rssi;
        app->results[i].detected = (rssi > RF_SCAN_THRESHOLD_DBM);

        /* Build freq/rssi strings early — needed by both logging paths */
        char freq_str[16];
        char rssi_str[12];
        snprintf(
            freq_str, sizeof(freq_str),
            "%lu.%03luMHz",
            (unsigned long)(freq / 1000000u),
            (unsigned long)((freq % 1000000u) / 1000u));
        snprintf(rssi_str, sizeof(rssi_str), "%.0fdBm", (double)rssi);

        if(app->results[i].detected) {
            bool was_detected = (app->signal_history[i] > 0);

            /* Increment history (clamp to avoid uint8 overflow) */
            if(app->signal_history[i] < 0xFFu) {
                app->signal_history[i]++;
            }

            if(!was_detected) {
                /* ── New detection session: transient signal ─── */
                app->detected_count++;
                audit_log_event("SubGhz", "SCAN", freq_str, rssi_str);

            } else if(app->signal_history[i] == RF_SCAN_PERSIST_MIN &&
                      !(app->persistent_mask & (1u << i))) {
                /* ── Crossed persistence threshold for first time ─── */
                app->persistent_mask |= (1u << i);
                audit_log_event("SubGhz", "PERSIST", freq_str, rssi_str);
                /* detected_count already incremented on first detection */
            }
            /* All subsequent detections in same session: silent */

        } else {
            /* Signal disappeared — end detection session */
            app->signal_history[i] = 0;
            app->persistent_mask  &= ~(1u << i);
        }
    }

    /* Return CC1101 to idle */
    furi_hal_subghz_idle();

    /* Notify UI thread that the sweep is done */
    if(!app->scan_stop) {
        view_dispatcher_send_custom_event(
            app->view_dispatcher, (uint32_t)RfScannerEventScanDone);
    }

    app->worker_running = false;
    return 0;
}

/* ── Public API ────────────────────────────────────────────────────────── */

void rf_scanner_worker_start(RfScannerApp* app) {
    furi_assert(app);
    furi_assert(!app->worker_running);

    /*
     * Join any previous run so FreeRTOS can reclaim the old task before we
     * call furi_thread_start() again.  On the very first start the thread is
     * in FuriThreadStateStopped and join returns immediately.
     */
    furi_thread_join(app->worker);

    app->scan_stop      = false;
    app->worker_running = true;
    furi_thread_start(app->worker);
}

void rf_scanner_worker_stop(RfScannerApp* app) {
    furi_assert(app);
    if(app->worker_running) {
        app->scan_stop = true;
        furi_thread_join(app->worker);
        /* worker_running is cleared by the thread itself before returning */
    }
}
