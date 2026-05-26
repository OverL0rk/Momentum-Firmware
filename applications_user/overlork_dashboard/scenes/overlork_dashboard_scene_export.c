/**
 * @file overlork_dashboard_scene_export.c
 * @brief Export JSON scene — converts today's audit CSV to a machine-readable
 *        JSON file at /ext/audit/audit-YYYY-MM-DD.json.
 *
 * Output format (single line, minified):
 *   {
 *     "date":  "2026-05-26",
 *     "gen":   "OverL0rk v0.2",
 *     "events": [
 *       {"ts":"14:32:11","sub":"NFC","op":"READ","id":"04:AB:CD","det":"NTAG215"},
 *       ...
 *     ]
 *   }
 *
 * CSV input format (audit/audit.h):
 *   YYYY-MM-DD HH:MM:SS,subsystem,operation,identifier,details\n
 *
 * Use-cases:
 *   - Feed into flipper-mcp / local AI via SD-card file transfer
 *   - Post-session analysis with jq or any JSON parser
 *   - Pattern-analyzer can consume JSON directly in future
 *
 * Author : Eudys Ramirez (@OverL0rk)
 */

#include "../overlork_dashboard_i.h"
#include <audit/audit.h>
#include <furi_hal_rtc.h>
#include <storage/storage.h>
#include <datetime/datetime.h>
#include <string.h>

/* ── Constants ───────────────────────────────────────────────────────────── */

/** Max bytes consumed from the CSV (protects heap on huge logs) */
#define OD_EXPORT_MAX_CSV_BYTES 32768u

/** Approximate max bytes per JSON event entry */
#define OD_EXPORT_EVENT_MAX_JSON 128u

/** Maximum events we will encode (prevents unbounded JSON growth) */
#define OD_EXPORT_MAX_EVENTS 512u

/* ── Helpers ─────────────────────────────────────────────────────────────── */

/**
 * JSON-escape a C string into dst (max dst_size bytes including NUL).
 * Replaces: \ → \\  " → \"  control chars → \uXXXX equivalent spaces.
 * Returns the number of bytes written (excluding NUL).
 */
static size_t json_escape(char* dst, size_t dst_size, const char* src) {
    size_t wi = 0;
    for(size_t ri = 0; src[ri] && wi + 2 < dst_size; ri++) {
        unsigned char c = (unsigned char)src[ri];
        if(c == '"' || c == '\\') {
            if(wi + 3 >= dst_size) break;
            dst[wi++] = '\\';
            dst[wi++] = (char)c;
        } else if(c < 0x20u) {
            /* Replace control chars with space to keep output printable */
            dst[wi++] = ' ';
        } else {
            dst[wi++] = (char)c;
        }
    }
    dst[wi] = '\0';
    return wi;
}

/**
 * Parse one CSV line from the audit log.
 *
 * Expected format:
 *   YYYY-MM-DD HH:MM:SS,subsystem,operation,identifier,details
 *
 * Returns true if all 5 fields were parsed; false on malformed line.
 * All output buffers are NUL-terminated; max sizes:
 *   ts[9], sub[16], op[16], id[64], det[64]
 */
static bool parse_csv_line(
    const char* line,
    char        ts[9],
    char        sub[16],
    char        op[16],
    char        id[64],
    char        det[64]) {
    /* Field 0: "YYYY-MM-DD HH:MM:SS" — we only keep the time part (index 11..18) */
    if(strlen(line) < 20) return false;
    /* Check date-time separator at index 10 */
    if(line[10] != ' ' || line[19] != ',') return false;

    /* Extract HH:MM:SS (8 chars starting at offset 11) */
    memcpy(ts, line + 11, 8);
    ts[8] = '\0';

    /* Walk remaining CSV fields */
    const char* p = line + 20; /* points just after the first comma */

    /* Helper macro: copy next comma-delimited token into buf[size] */
#define NEXT_FIELD(buf, size)                              \
    do {                                                   \
        const char* end = strchr(p, ',');                  \
        if(!end) end = p + strlen(p); /* last field */     \
        size_t len = (size_t)(end - p);                    \
        if(len >= (size)) len = (size)-1;                  \
        memcpy((buf), p, len);                             \
        (buf)[len] = '\0';                                 \
        p = (*end == ',') ? end + 1 : end;                 \
    } while(0)

    NEXT_FIELD(sub, 16);
    NEXT_FIELD(op, 16);
    NEXT_FIELD(id, 64);
    NEXT_FIELD(det, 64);

#undef NEXT_FIELD

    return sub[0] != '\0';
}

/* ── Core export logic ───────────────────────────────────────────────────── */

typedef enum {
    OdExportResultOk,
    OdExportResultNoCsv,
    OdExportResultReadError,
    OdExportResultWriteError,
} OdExportResult;

/**
 * Read today's CSV audit log, convert every line to a JSON event object,
 * and write the result to /ext/audit/audit-YYYY-MM-DD.json.
 *
 * Fills out_json_path (size ≥ 64) with the path on success.
 * Returns OdExportResult.
 */
static OdExportResult od_do_export(char* out_json_path, size_t path_size, uint32_t* out_count) {
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);

    /* Build CSV + JSON paths */
    char csv_path[64];
    snprintf(
        csv_path,
        sizeof(csv_path),
        AUDIT_BASE_PATH "/audit-%04u-%02u-%02u.csv",
        (unsigned)dt.year,
        (unsigned)dt.month,
        (unsigned)dt.day);

    snprintf(
        out_json_path,
        path_size,
        AUDIT_BASE_PATH "/audit-%04u-%02u-%02u.json",
        (unsigned)dt.year,
        (unsigned)dt.month,
        (unsigned)dt.day);

    Storage* storage = furi_record_open(RECORD_STORAGE);

    /* Verify CSV exists */
    if(!storage_file_exists(storage, csv_path)) {
        furi_record_close(RECORD_STORAGE);
        return OdExportResultNoCsv;
    }

    /* Read entire CSV into a FuriString (bounded by OD_EXPORT_MAX_CSV_BYTES) */
    File* csv_file = storage_file_alloc(storage);
    if(!storage_file_open(csv_file, csv_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(csv_file);
        furi_record_close(RECORD_STORAGE);
        return OdExportResultReadError;
    }

    FuriString* csv_buf = furi_string_alloc();
    {
        uint8_t  chunk[256];
        uint32_t total_read = 0;
        while(total_read < OD_EXPORT_MAX_CSV_BYTES) {
            uint16_t got = storage_file_read(csv_file, chunk, sizeof(chunk) - 1);
            if(got == 0) break;
            chunk[got] = '\0';
            furi_string_cat_str(csv_buf, (const char*)chunk);
            total_read += got;
        }
    }
    storage_file_close(csv_file);
    storage_file_free(csv_file);

    /* Open JSON output file for writing (overwrite if exists) */
    File* json_file = storage_file_alloc(storage);
    if(!storage_file_open(json_file, out_json_path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        furi_string_free(csv_buf);
        storage_file_free(json_file);
        furi_record_close(RECORD_STORAGE);
        return OdExportResultWriteError;
    }

    /* Write JSON header */
    {
        char hdr[128];
        int  hdr_len = snprintf(
            hdr,
            sizeof(hdr),
            "{\"date\":\"%04u-%02u-%02u\",\"gen\":\"OverL0rk v0.2\",\"events\":[",
            (unsigned)dt.year,
            (unsigned)dt.month,
            (unsigned)dt.day);
        if(storage_file_write(json_file, hdr, (uint16_t)hdr_len) != (uint16_t)hdr_len) {
            storage_file_close(json_file);
            storage_file_free(json_file);
            furi_string_free(csv_buf);
            furi_record_close(RECORD_STORAGE);
            return OdExportResultWriteError;
        }
    }

    /* Iterate over CSV lines and emit JSON event objects */
    const char* raw    = furi_string_get_cstr(csv_buf);
    uint32_t    count  = 0;
    bool        first  = true;
    bool        wr_err = false;

    while(*raw && count < OD_EXPORT_MAX_EVENTS) {
        /* Find end of current line */
        const char* nl = strchr(raw, '\n');
        size_t      line_len;
        char        line[256];

        if(nl) {
            line_len = (size_t)(nl - raw);
        } else {
            line_len = strlen(raw);
        }

        /* Skip empty lines */
        if(line_len == 0) {
            raw = nl ? nl + 1 : raw + line_len;
            continue;
        }

        /* Copy line (strip trailing \r if any) */
        size_t copy_len = (line_len < sizeof(line) - 1) ? line_len : sizeof(line) - 1;
        memcpy(line, raw, copy_len);
        if(copy_len > 0 && line[copy_len - 1] == '\r') copy_len--;
        line[copy_len] = '\0';

        /* Parse CSV fields */
        char ts[9], sub[16], op[16], id[64], det[64];
        if(parse_csv_line(line, ts, sub, op, id, det)) {
            /* JSON-escape the variable fields */
            char esc_id[128], esc_det[128];
            json_escape(esc_id, sizeof(esc_id), id);
            json_escape(esc_det, sizeof(esc_det), det);

            /* Build event JSON object */
            char   ev[OD_EXPORT_EVENT_MAX_JSON];
            int    ev_len;
            if(first) {
                ev_len = snprintf(
                    ev,
                    sizeof(ev),
                    "{\"ts\":\"%s\",\"sub\":\"%s\",\"op\":\"%s\",\"id\":\"%s\",\"det\":\"%s\"}",
                    ts, sub, op, esc_id, esc_det);
                first = false;
            } else {
                ev_len = snprintf(
                    ev,
                    sizeof(ev),
                    ",{\"ts\":\"%s\",\"sub\":\"%s\",\"op\":\"%s\",\"id\":\"%s\",\"det\":\"%s\"}",
                    ts, sub, op, esc_id, esc_det);
            }

            if(ev_len > 0) {
                uint16_t to_write = (uint16_t)ev_len;
                if(storage_file_write(json_file, ev, to_write) != to_write) {
                    wr_err = true;
                    break;
                }
                count++;
            }
        }

        raw = nl ? nl + 1 : raw + line_len;
    }

    furi_string_free(csv_buf);

    if(!wr_err) {
        /* Close JSON array and root object */
        const char* footer = "]}";
        uint16_t    flen   = (uint16_t)strlen(footer);
        if(storage_file_write(json_file, footer, flen) != flen) {
            wr_err = true;
        }
    }

    storage_file_close(json_file);
    storage_file_free(json_file);
    furi_record_close(RECORD_STORAGE);

    if(wr_err) return OdExportResultWriteError;

    *out_count = count;
    return OdExportResultOk;
}

/* ── Scene ───────────────────────────────────────────────────────────────── */

/*
 * Result is stored here so widget strings (which are pointer-referenced)
 * survive the on_enter call.  Static is fine — only one export scene exists.
 */
static char s_export_line1[48]; /* "Done! N events"  or  error message         */
static char s_export_line2[64]; /* JSON path (truncated to fit widget width)    */

void overlork_dashboard_scene_export_on_enter(void* context) {
    OdApp*  app    = context;
    Widget* widget = app->widget;

    widget_reset(widget);

    /* Show header immediately */
    widget_add_string_element(
        widget, 64, 4, AlignCenter, AlignTop, FontPrimary, "Export JSON");

    /* "Working..." placeholder visible during the export */
    widget_add_string_element(
        widget, 64, 24, AlignCenter, AlignTop, FontSecondary, "Working...");
    widget_add_string_element(
        widget, 64, 36, AlignCenter, AlignTop, FontSecondary, "Please wait");

    view_dispatcher_switch_to_view(app->view_dispatcher, OdViewWidget);

    /* --- Perform the export synchronously ---
     *
     * The Flipper GUI freezes during this call, but the export is fast
     * (<100ms for typical log sizes).  An async version would require a
     * worker thread; the complexity is not warranted here.
     */
    char     json_path[64] = {0};
    uint32_t event_count   = 0;
    OdExportResult result  = od_do_export(json_path, sizeof(json_path), &event_count);

    /* Re-build the widget with the actual result */
    widget_reset(widget);

    widget_add_string_element(
        widget, 64, 4, AlignCenter, AlignTop, FontPrimary, "Export JSON");

    switch(result) {
    case OdExportResultOk:
        snprintf(s_export_line1, sizeof(s_export_line1), "Done! %lu events", (unsigned long)event_count);
        /* Show only the filename portion to fit in 128px */
        {
            const char* fname = strrchr(json_path, '/');
            snprintf(
                s_export_line2,
                sizeof(s_export_line2),
                "%s",
                fname ? fname + 1 : json_path);
        }
        widget_add_string_element(
            widget, 64, 20, AlignCenter, AlignTop, FontSecondary, s_export_line1);
        widget_add_string_element(
            widget, 64, 32, AlignCenter, AlignTop, FontSecondary, s_export_line2);
        widget_add_string_element(
            widget, 64, 48, AlignCenter, AlignTop, FontSecondary, "/ext/audit/");
        break;

    case OdExportResultNoCsv:
        snprintf(s_export_line1, sizeof(s_export_line1), "No log for today");
        snprintf(s_export_line2, sizeof(s_export_line2), "Generate events first");
        widget_add_string_element(
            widget, 64, 22, AlignCenter, AlignTop, FontSecondary, s_export_line1);
        widget_add_string_element(
            widget, 64, 34, AlignCenter, AlignTop, FontSecondary, s_export_line2);
        break;

    case OdExportResultReadError:
        snprintf(s_export_line1, sizeof(s_export_line1), "Read error");
        snprintf(s_export_line2, sizeof(s_export_line2), "Check SD card");
        widget_add_string_element(
            widget, 64, 22, AlignCenter, AlignTop, FontSecondary, s_export_line1);
        widget_add_string_element(
            widget, 64, 34, AlignCenter, AlignTop, FontSecondary, s_export_line2);
        break;

    case OdExportResultWriteError:
        snprintf(s_export_line1, sizeof(s_export_line1), "Write error");
        snprintf(s_export_line2, sizeof(s_export_line2), "Check SD space");
        widget_add_string_element(
            widget, 64, 22, AlignCenter, AlignTop, FontSecondary, s_export_line1);
        widget_add_string_element(
            widget, 64, 34, AlignCenter, AlignTop, FontSecondary, s_export_line2);
        break;
    }

    /* Common footer */
    widget_add_string_element(
        widget, 64, 58, AlignCenter, AlignTop, FontSecondary, "[ Back ]");
}

bool overlork_dashboard_scene_export_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void overlork_dashboard_scene_export_on_exit(void* context) {
    OdApp* app = context;
    widget_reset(app->widget);
}
