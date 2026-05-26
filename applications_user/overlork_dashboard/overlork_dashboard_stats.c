/**
 * @file overlork_dashboard_stats.c
 * @brief Parses today's audit CSV and computes summary statistics.
 *
 * Audit CSV format (5 fields):
 *   YYYY-MM-DD HH:MM:SS,subsystem,operation,identifier,details
 * The first line of each new file is a header row starting with "timestamp".
 *
 * Reads the file in 256-byte chunks for efficiency. Since the file is
 * append-only and written in chronological order, the last valid line
 * always represents the most-recent event — no epoch comparison needed.
 */

#include "overlork_dashboard_i.h"
#include <audit/audit.h>
#include <furi_hal_power.h>
#include <furi_hal_rtc.h>
#include <storage/storage.h>
#include <datetime/datetime.h>
#include <string.h>

#define OD_LINE_MAX   160  /* wide enough for max CSV line (5 × 64 + separators) */
#define OD_CHUNK_SIZE 256

/**
 * Process one CSV line and update the stats accumulator.
 *
 * @param stats  Accumulator to update.
 * @param line   Mutable buffer (will be tokenised in-place).
 */
static void od_process_line(OdStats* stats, char* line) {
    /* Skip header: "timestamp,subsystem,..." */
    if(strncmp(line, "timestamp", 9) == 0) return;

    /* Field 0: timestamp "YYYY-MM-DD HH:MM:SS" */
    char* c1 = strchr(line, ',');
    if(!c1) return;
    *c1 = '\0';
    char* timestamp = line;

    /* Extract the time part after the space */
    char* space = strchr(timestamp, ' ');
    if(!space) return;
    const char* time_str = space + 1; /* "HH:MM:SS" */
    if(strlen(time_str) < 8) return;

    /* Field 1: subsystem */
    char* subsys = c1 + 1;
    char* c2 = strchr(subsys, ',');
    if(!c2) return;
    *c2 = '\0';

    /* Accumulate counts */
    stats->total++;
    if(strcmp(subsys, "NFC") == 0) {
        stats->nfc++;
    } else if(strcmp(subsys, "SubGhz") == 0 || strcmp(subsys, "SubGHz") == 0) {
        /* accept both capitalisations for robustness */
        stats->subghz++;
    } else if(strcmp(subsys, "IR") == 0) {
        stats->ir++;
    } else if(strcmp(subsys, "BadKB") == 0) {
        stats->badkb++;
    } else if(strcmp(subsys, "LFRFID") == 0) {
        stats->lfrfid++;
    } else if(strcmp(subsys, "iButton") == 0) {
        stats->ibutton++;
    }

    /*
     * Track the most-recent event.
     * Because the file is append-only and written in chronological order,
     * each subsequent line is always newer — simply overwrite.
     */
    memcpy(stats->last_time, time_str, 8);
    stats->last_time[8] = '\0';
    strncpy(stats->last_subsys, subsys, sizeof(stats->last_subsys) - 1);
    stats->last_subsys[sizeof(stats->last_subsys) - 1] = '\0';
}

void od_compute_stats(OdStats* stats) {
    memset(stats, 0, sizeof(OdStats));
    stats->battery_pct = furi_hal_power_get_pct();

    /* Defaults shown when the log is missing or empty */
    strncpy(stats->last_time, "--:--:--", sizeof(stats->last_time) - 1);
    strncpy(stats->last_subsys, "N/A", sizeof(stats->last_subsys) - 1);

    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);

    FuriString* path = furi_string_alloc_printf(
        AUDIT_BASE_PATH "/audit-%04u-%02u-%02u.csv",
        (unsigned)dt.year,
        (unsigned)dt.month,
        (unsigned)dt.day);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File*    file    = storage_file_alloc(storage);

    if(!storage_file_open(file, furi_string_get_cstr(path), FSAM_READ, FSOM_OPEN_EXISTING)) {
        /* No file yet — leave defaults, battery already set */
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        furi_string_free(path);
        return;
    }

    uint8_t  chunk[OD_CHUNK_SIZE];
    char     line[OD_LINE_MAX];
    uint16_t line_pos = 0;

    while(true) {
        uint16_t got = storage_file_read(file, chunk, sizeof(chunk));
        if(got == 0) break;

        for(uint16_t i = 0; i < got; i++) {
            char ch = (char)chunk[i];
            if(ch == '\n' || line_pos >= (uint16_t)(OD_LINE_MAX - 1)) {
                line[line_pos] = '\0';
                if(line_pos > 4) od_process_line(stats, line);
                line_pos = 0;
            } else if(ch != '\r') {
                line[line_pos++] = ch;
            }
        }
    }

    /* Flush any trailing line that ends without a newline */
    if(line_pos > 4) {
        line[line_pos] = '\0';
        od_process_line(stats, line);
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    furi_string_free(path);
}
