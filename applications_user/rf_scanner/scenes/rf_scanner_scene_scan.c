/**
 * @file rf_scanner_scene_scan.c
 * @brief Scan scene — launches the worker thread, displays live status,
 *        and renders the per-frequency RSSI table when the sweep finishes.
 *
 * Single-shot mode ("Scan Now"):
 *   Shows results once; Back returns to the start menu.
 *
 * Watch mode ("Watch (5s)"):
 *   After results are displayed, a RF_SCAN_WATCH_INTERVAL_MS timer fires and
 *   automatically triggers a new sweep.  The footer shows the interval.
 *   Pressing Back at any time stops the timer and returns to the menu.
 *
 * Display format (TextBoxFontText, ~18 chars/line):
 *
 *   RF Scanner [Watch]       ← header changes by mode
 *   315.000 -95dBm
 *   433.920 -83dBm!          ← '!' = above RF_SCAN_THRESHOLD_DBM
 *   868.350 -91dBm
 *   915.000 -88dBm!
 *   ──────────────
 *   Found: 2 | Next: 5s     ← watch mode footer
 *   Found: 2 signals         ← single-shot footer
 */

#include "../rf_scanner_i.h"

/** Build the post-scan result string into app->display_text */
static void rfs_build_result_text(RfScannerApp* app) {
    if(app->watch_mode) {
        furi_string_set(app->display_text, "RF Scanner [Watch]\n");
    } else {
        furi_string_set(app->display_text, "RF Scanner v0.1\n");
    }

    uint8_t persistent_count = 0;

    for(uint8_t i = 0; i < RF_SCAN_FREQ_COUNT; i++) {
        uint32_t mhz = app->results[i].freq_hz / 1000000u;
        uint32_t khz = (app->results[i].freq_hz % 1000000u) / 1000u;

        /*
         * Markers (inspired by ProtoView's signal annotation style):
         *   "*" — persistent source (same transmitter across 3+ sweeps)
         *         → logged as SubGhz/PERSIST → OlRuleActionAlert
         *   "!" — transient detection (above threshold but not persistent)
         *   ""  — below threshold
         */
        const char* marker = "";
        if(app->persistent_mask & (1u << i)) {
            marker = "*";
            persistent_count++;
        } else if(app->results[i].detected) {
            marker = "!";
        }

        furi_string_cat_printf(
            app->display_text,
            "%lu.%03lu %.0fdBm%s\n",
            (unsigned long)mhz,
            (unsigned long)khz,
            (double)app->results[i].rssi,
            marker);
    }

    if(app->watch_mode) {
        if(persistent_count > 0) {
            furi_string_cat_printf(
                app->display_text,
                "!=%u *=%u [PERSIST]",
                app->detected_count,
                (unsigned)persistent_count);
        } else {
            furi_string_cat_printf(
                app->display_text,
                "Found:%u | Next:%us",
                app->detected_count,
                (unsigned)(RF_SCAN_WATCH_INTERVAL_MS / 1000u));
        }
    } else {
        furi_string_cat_printf(
            app->display_text,
            "Found: %u signal%s",
            app->detected_count,
            app->detected_count == 1u ? "" : "s");
    }
}

/** Push current display_text into the TextBox widget */
static void rfs_refresh_textbox(RfScannerApp* app) {
    TextBox* tb = app->text_box;
    text_box_reset(tb);
    text_box_set_font(tb, TextBoxFontText);
    text_box_set_focus(tb, TextBoxFocusEnd);
    text_box_set_text(tb, furi_string_get_cstr(app->display_text));
}

/* ── Scene callbacks ──────────────────────────────────────────────────── */

void rf_scanner_scene_scan_on_enter(void* context) {
    RfScannerApp* app = context;

    /* Show placeholder while the worker runs */
    if(app->watch_mode) {
        furi_string_set(
            app->display_text,
            "RF Scanner [Watch]\n"
            "\n"
            "Scanning 4 freqs...\n"
            "Please wait ~1s");
    } else {
        furi_string_set(
            app->display_text,
            "RF Scanner v0.1\n"
            "\n"
            "Scanning 4 freqs...\n"
            "Please wait ~1s");
    }
    rfs_refresh_textbox(app);

    view_dispatcher_switch_to_view(app->view_dispatcher, RfScannerViewTextBox);

    /* Kick off the RF sweep */
    rf_scanner_worker_start(app);
}

bool rf_scanner_scene_scan_on_event(void* context, SceneManagerEvent event) {
    RfScannerApp* app      = context;
    bool          consumed = false;

    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == (uint32_t)RfScannerEventScanDone) {
        /* Worker finished — render results */
        rfs_build_result_text(app);
        rfs_refresh_textbox(app);

        /* Notify: red blink if signals detected, cyan otherwise */
        if(app->detected_count > 0) {
            notification_message(app->notifications, &sequence_blink_red_100);
        } else {
            notification_message(app->notifications, &sequence_blink_cyan_100);
        }

        /* In watch mode schedule the next sweep after the interval */
        if(app->watch_mode) {
            furi_timer_start(app->watch_timer, RF_SCAN_WATCH_INTERVAL_MS);
        }

        consumed = true;
    } else if(event.event == (uint32_t)RfScannerEventWatchTick) {
        /* Watch interval elapsed — start a new sweep immediately */
        furi_string_set(
            app->display_text,
            "RF Scanner [Watch]\n"
            "\n"
            "Scanning 4 freqs...\n"
            "Please wait ~1s");
        rfs_refresh_textbox(app);
        rf_scanner_worker_start(app);

        consumed = true;
    }

    return consumed;
}

void rf_scanner_scene_scan_on_exit(void* context) {
    RfScannerApp* app = context;

    /* Cancel pending watch tick before leaving the scene */
    furi_timer_stop(app->watch_timer);

    /* Stop the worker if it is still running (e.g. user pressed Back) */
    rf_scanner_worker_stop(app);

    text_box_reset(app->text_box);
}
