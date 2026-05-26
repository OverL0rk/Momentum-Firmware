/**
 * @file flipper_guard_scene_watch.c
 * @brief v0.3 — Live audit monitor with severity status footer.
 *
 * Improvements over v0.2:
 *   - Session event counter: counts new events that arrive while Guard is open.
 *   - Severity status footer: always visible at the bottom of the TextBox.
 *     Format: "Guard +N | ALERT" where N = new events this session.
 *   - Session-wide severity tracking: footer shows the worst severity seen
 *     since entering Watch, not just the last tick's severity.
 *
 * TextBox display layout (bottom-focused, scrollable):
 *   ...
 *   14:32:11,NFC,READ,04:AB:CD,NTAG215
 *   14:33:02,SubGhz,SCAN,315.000MHz,-83dBm
 *   ──────────────
 *   Guard +2 | ALERT
 *
 * On each 1-second timer tick (FGuardEventPoll):
 *   1. Stat the file; if it grew, read only the new bytes.
 *   2. Parse complete new lines into OlEvents; count valid events.
 *   3. Pass each event through ol_rule_check() — keep highest severity.
 *   4. Update session_events and session_max_sev.
 *   5. Call ol_notify() with batch severity (Info=cyan … Critical=magenta+vibro).
 *   6. Reload the tail + rebuild footer in TextBox.
 */

#include "../flipper_guard_i.h"
#include <audit/audit.h>
#include <furi_hal_rtc.h>
#include <storage/storage.h>
#include <datetime/datetime.h>
#include <string.h>

#define FG_TAIL_BYTES 1900u  /* leave room for footer when total < 2000 bytes */
#define FG_POLL_MS    1000u
#define FG_LINE_MAX   256u

/* ── helpers ──────────────────────────────────────────────────────────── */

static void fg_today_path(FuriString* out) {
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    furi_string_printf(
        out,
        AUDIT_BASE_PATH "/audit-%04u-%02u-%02u.csv",
        (unsigned)dt.year,
        (unsigned)dt.month,
        (unsigned)dt.day);
}

/** Map a severity value to a short display label */
static const char* fg_sev_label(OlNotifySeverity sev) {
    switch(sev) {
    case OlNotifyInfo:     return "OK";
    case OlNotifyWarn:     return "WARN";
    case OlNotifyAlert:    return "ALERT";
    case OlNotifyCritical: return "CRIT!";
    default:               return "?";
    }
}

/* ── tail loader ─────────────────────────────────────────────────────── */

static void fg_load_tail(FGuardApp* app) {
    FuriString* path = furi_string_alloc();
    fg_today_path(path);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File*    file    = storage_file_alloc(storage);

    furi_string_reset(app->watch_text);

    if(!storage_file_exists(storage, furi_string_get_cstr(path))) {
        furi_string_set(
            app->watch_text,
            "[FlipperGuard v0.3]\n"
            "Waiting for events...\n\n"
            "Enable audit in\n"
            "Audit Viewer.");
        app->last_file_size     = 0;
        app->last_parsed_offset = 0;
    } else if(
        storage_file_open(file, furi_string_get_cstr(path), FSAM_READ, FSOM_OPEN_EXISTING)) {
        app->last_file_size = storage_file_size(file);

        /* On first load, skip rule-checking for existing content */
        if(app->last_parsed_offset == 0) {
            app->last_parsed_offset = app->last_file_size;
        }

        uint64_t offset = (app->last_file_size > (uint64_t)FG_TAIL_BYTES)
                              ? (app->last_file_size - (uint64_t)FG_TAIL_BYTES)
                              : 0;
        storage_file_seek(file, (uint32_t)offset, true);

        uint8_t buf[256];
        while(true) {
            uint16_t got = storage_file_read(file, buf, sizeof(buf) - 1);
            if(got == 0) break;
            buf[got] = '\0';
            furi_string_cat_str(app->watch_text, (const char*)buf);
        }
        storage_file_close(file);

        if(furi_string_empty(app->watch_text)) {
            furi_string_set(app->watch_text, "Log is empty. Waiting...");
        }
    } else {
        furi_string_set(app->watch_text, "Cannot open log.\nCheck SD card.");
        app->last_file_size     = 0;
        app->last_parsed_offset = 0;
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    furi_string_free(path);
}

/**
 * Refresh the TextBox with the CSV tail plus a v0.3 severity status footer
 * at the bottom.  Since TextBoxFocusEnd scrolls to the last line, the footer
 * is always visible without any user interaction.
 *
 * Display layout:
 *   {csv tail lines}
 *   ──────────────
 *   Guard +N | SEV
 */
static void fg_refresh_textbox(FGuardApp* app) {
    TextBox* tb = app->text_box;
    text_box_reset(tb);
    text_box_set_font(tb, TextBoxFontText);
    text_box_set_focus(tb, TextBoxFocusEnd);

    /* Build complete display: csv tail + divider + status footer */
    FuriString* display = furi_string_alloc_printf(
        "%s\n"
        "--------------\n"
        "Guard +%lu | %s",
        furi_string_get_cstr(app->watch_text),
        (unsigned long)app->session_events,
        fg_sev_label(app->session_max_sev));

    text_box_set_text(tb, furi_string_get_cstr(display));
    furi_string_free(display);
}

/* ── incremental parser with rule engine ─────────────────────────────── */

/**
 * Read bytes from last_parsed_offset to current EOF.
 * For each complete CSV line:
 *   - Parse into OlEvent via ol_event_from_csv_line()
 *   - Run through ol_rule_check(); track highest severity
 *   - Increment app->session_events for every valid event
 *
 * Updates app->last_parsed_offset and app->session_events in-place.
 * Returns the highest severity seen across all new events (OlNotifyInfo if none).
 */
static OlNotifySeverity fg_parse_new_events(FGuardApp* app, const char* path_cstr) {
    OlNotifySeverity highest = OlNotifyInfo;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File*    file    = storage_file_alloc(storage);

    if(!storage_file_open(file, path_cstr, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return highest;
    }

    uint64_t new_size = storage_file_size(file);
    if(new_size <= app->last_parsed_offset) {
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return highest;
    }

    storage_file_seek(file, (uint32_t)app->last_parsed_offset, true);

    uint8_t  chunk[256];
    char     line[FG_LINE_MAX];
    uint16_t line_pos = 0;
    OlEvent  ev;

    while(true) {
        uint16_t got = storage_file_read(file, chunk, sizeof(chunk));
        if(got == 0) break;

        for(uint16_t i = 0; i < got; i++) {
            char ch = (char)chunk[i];
            if(ch == '\n' || line_pos >= (uint16_t)(FG_LINE_MAX - 1)) {
                line[line_pos] = '\0';
                if(line_pos > 4) {
                    if(ol_event_from_csv_line(line, &ev)) {
                        /* Count every successfully parsed event */
                        app->session_events++;

                        const OlRule* rule = ol_rule_check(&ev);
                        if(rule) {
                            OlNotifySeverity rule_sev = (OlNotifySeverity)rule->action;
                            if(rule_sev > highest) {
                                highest = rule_sev;
                            }
                            /* Update session-wide maximum severity */
                            if(rule_sev > app->session_max_sev) {
                                app->session_max_sev = rule_sev;
                            }
                        }
                    }
                }
                line_pos = 0;
            } else if(ch != '\r') {
                line[line_pos++] = ch;
            }
        }
    }

    app->last_parsed_offset = new_size;

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return highest;
}

/* ── scene callbacks ──────────────────────────────────────────────────── */

void flipper_guard_scene_watch_on_enter(void* context) {
    FGuardApp* app = context;

    /* Reset session counters for this watch session */
    app->session_events   = 0;
    app->session_max_sev  = OlNotifyInfo;
    app->last_parsed_offset = 0;

    fg_load_tail(app);
    fg_refresh_textbox(app);

    view_dispatcher_switch_to_view(app->view_dispatcher, FGuardViewTextBox);
    furi_timer_start(app->poll_timer, FG_POLL_MS);
}

bool flipper_guard_scene_watch_on_event(void* context, SceneManagerEvent event) {
    FGuardApp* app      = context;
    bool       consumed = false;

    if(event.type == SceneManagerEventTypeCustom &&
       event.event == (uint32_t)FGuardEventPoll) {
        FuriString* path = furi_string_alloc();
        fg_today_path(path);

        Storage* storage = furi_record_open(RECORD_STORAGE);
        FileInfo info;
        info.size = 0;
        storage_common_stat(storage, furi_string_get_cstr(path), &info);
        furi_record_close(RECORD_STORAGE);

        uint64_t new_size = info.size;

        if(new_size > app->last_file_size) {
            /* Parse new lines — updates session_events and session_max_sev */
            OlNotifySeverity batch_sev =
                fg_parse_new_events(app, furi_string_get_cstr(path));

            /* Reload display tail and rebuild textbox with updated footer */
            fg_load_tail(app);
            fg_refresh_textbox(app);

            /* LED/vibro for this batch (independent of session max) */
            ol_notify(app->notifications, batch_sev);
        }

        furi_string_free(path);
        consumed = true;
    }

    return consumed;
}

void flipper_guard_scene_watch_on_exit(void* context) {
    FGuardApp* app = context;
    furi_timer_stop(app->poll_timer);
    text_box_reset(app->text_box);
}
