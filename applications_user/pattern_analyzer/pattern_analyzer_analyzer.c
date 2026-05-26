/**
 * @file pattern_analyzer_analyzer.c
 * @brief v0.3 — Analysis engine for the Pattern Analyzer FAP.
 *
 * New in v0.2:
 *   - Buffered chunk reads (was 1-byte at a time)
 *   - Cross-protocol correlation (same ID in >1 subsystem)
 *   - Top-identifier frequency ranking (all subsystems)
 *   - Short-burst detection (>=3 events in 5 s)
 *   - Report export to /ext/audit/report-YYYY-MM-DD.txt
 *
 * New in v0.3 (community research integration):
 *   - Relay attack detection: credential read + SubGHz within 30s
 *     (inspired by iClass/NFC relay research from DEF CON 32 / bettse/seader)
 *   - Night activity flagging: events between midnight and 05:00 local time
 *   - Threat diversity profile: which of the 7 attack vectors were active
 *     and how many (inspired by the concept of multi-vector operation
 *     fingerprinting from ProtoView/Kashmir54 research)
 *
 * Author : Eudys Ramirez (@OverL0rk)
 */

#include "pattern_analyzer_i.h"

#include <furi_hal_rtc.h>
#include <storage/storage.h>
#include <datetime/datetime.h>
#include <stdlib.h>
#include <string.h>

#define LINE_BUF    256
#define CHUNK_SIZE  256

/* ── helpers ──────────────────────────────────────────────────────────── */

/**
 * Convert "YYYY-MM-DD HH:MM:SS" string to a UNIX timestamp.
 * Returns 0 on parse failure.
 */
static uint32_t parse_epoch(const char* ts) {
    int y, mo, d, h, mi, s;
    if(sscanf(ts, "%d-%d-%d %d:%d:%d", &y, &mo, &d, &h, &mi, &s) != 6) return 0;
    DateTime dt = {
        .year   = (uint16_t)y,
        .month  = (uint8_t)mo,
        .day    = (uint8_t)d,
        .hour   = (uint8_t)h,
        .minute = (uint8_t)mi,
        .second = (uint8_t)s,
    };
    return datetime_datetime_to_timestamp(&dt);
}

/**
 * Tokenise a mutable CSV line (up to 5 fields) in-place.
 * Returns true when at least 4 fields (indices 0-3) are present.
 */
static bool split_csv_line(char* line, char* fields[5]) {
    size_t idx = 0;
    fields[0] = line;
    for(char* p = line; *p && idx < 4; p++) {
        if(*p == ',') {
            *p = '\0';
            fields[++idx] = p + 1;
        }
        if(*p == '\n' || *p == '\r') *p = '\0';
    }
    return idx >= 3; /* need fields 0-3 */
}

/* ── file loader ──────────────────────────────────────────────────────── */

static size_t pa_load_today(PatternAnalyzerApp* app) {
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);

    FuriString* path = furi_string_alloc_printf(
        PATTERN_ANALYZER_AUDIT_DIR "/audit-%04u-%02u-%02u.csv",
        (unsigned)dt.year,
        (unsigned)dt.month,
        (unsigned)dt.day);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File*    file    = storage_file_alloc(storage);
    app->event_count = 0;

    if(storage_file_open(file, furi_string_get_cstr(path), FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint8_t  chunk[CHUNK_SIZE];
        char     linebuf[LINE_BUF];
        uint16_t line_pos   = 0;
        bool     first_line = true; /* skip CSV header row */

        while(true) {
            uint16_t got = storage_file_read(file, chunk, sizeof(chunk));
            if(got == 0) break;

            for(uint16_t i = 0; i < got; i++) {
                char ch = (char)chunk[i];
                if(ch == '\n' || line_pos >= (uint16_t)(LINE_BUF - 1)) {
                    linebuf[line_pos] = '\0';
                    if(first_line) {
                        first_line = false;
                    } else if(
                        line_pos > 4 &&
                        app->event_count < PATTERN_ANALYZER_MAX_EVENTS) {
                        char*  fields[5] = {0};
                        if(split_csv_line(linebuf, fields)) {
                            AnalyzerEvent* ev = &app->events[app->event_count];
                            ev->epoch_seconds = parse_epoch(fields[0]);
                            strlcpy(ev->subsystem,  fields[1], sizeof(ev->subsystem));
                            strlcpy(ev->operation,  fields[2], sizeof(ev->operation));
                            strlcpy(ev->identifier, fields[3], sizeof(ev->identifier));
                            app->event_count++;
                        }
                    }
                    line_pos = 0;
                } else if(ch != '\r') {
                    linebuf[line_pos++] = ch;
                }
            }
        }

        /* Handle final line that ends without a newline */
        if(line_pos > 4 && !first_line &&
           app->event_count < PATTERN_ANALYZER_MAX_EVENTS) {
            linebuf[line_pos] = '\0';
            char* fields[5] = {0};
            if(split_csv_line(linebuf, fields)) {
                AnalyzerEvent* ev = &app->events[app->event_count];
                ev->epoch_seconds = parse_epoch(fields[0]);
                strlcpy(ev->subsystem,  fields[1], sizeof(ev->subsystem));
                strlcpy(ev->operation,  fields[2], sizeof(ev->operation));
                strlcpy(ev->identifier, fields[3], sizeof(ev->identifier));
                app->event_count++;
            }
        }

        storage_file_close(file);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    furi_string_free(path);
    return app->event_count;
}

/* ── hex helpers (for NFC UID binary comparisons) ───────────────────── */

static uint8_t hex_nibble(char c) {
    if(c >= '0' && c <= '9') return (uint8_t)(c - '0');
    if(c >= 'A' && c <= 'F') return (uint8_t)(c - 'A' + 10);
    if(c >= 'a' && c <= 'f') return (uint8_t)(c - 'a' + 10);
    return 0xFF;
}

static int hex_to_bytes(const char* hex, uint8_t* out, size_t out_max) {
    size_t hex_len = strlen(hex);
    if(hex_len % 2 != 0) return -1;
    size_t n = hex_len / 2;
    if(n > out_max) n = out_max;
    for(size_t i = 0; i < n; i++) {
        uint8_t hi = hex_nibble(hex[i * 2]);
        uint8_t lo = hex_nibble(hex[i * 2 + 1]);
        if(hi == 0xFF || lo == 0xFF) return -1;
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return (int)n;
}

/* ── analysis functions ──────────────────────────────────────────────── */

/** v0.1 — NFC sequential UID pairs (last byte differs by ≤4) */
static void analyze_sequential_uids(PatternAnalyzerApp* app) {
    uint32_t pairs = 0;
    for(size_t i = 0; i < app->event_count; i++) {
        if(strcmp(app->events[i].subsystem, "NFC") != 0) continue;
        uint8_t a[16];
        int a_len = hex_to_bytes(app->events[i].identifier, a, sizeof(a));
        if(a_len < 1) continue;

        for(size_t j = i + 1; j < app->event_count; j++) {
            if(strcmp(app->events[j].subsystem, "NFC") != 0) continue;
            uint8_t b[16];
            int b_len = hex_to_bytes(app->events[j].identifier, b, sizeof(b));
            if(b_len != a_len) continue;

            bool prefix_match = (a_len > 1) && memcmp(a, b, (size_t)(a_len - 1)) == 0;
            int diff = (int)a[a_len - 1] - (int)b[a_len - 1];
            if(diff < 0) diff = -diff;
            if(prefix_match && diff > 0 && diff <= 4) pairs++;
        }
    }
    furi_string_cat_printf(
        app->report,
        "[Sequential UIDs]\n%lu pair(s) differ only\nby last byte (<=4)\n%s\n\n",
        (unsigned long)pairs,
        pairs ? "-> possible fleet\n   generation" : "-> none detected");
}

/** v0.1 — NFC UID clusters sharing the same first 4 bytes */
static void analyze_shared_prefix(PatternAnalyzerApp* app) {
    bool seen[PATTERN_ANALYZER_MAX_EVENTS];
    memset(seen, 0, sizeof(seen));
    uint32_t groups_found = 0;
    char     first_prefix_hex[9] = {0};

    for(size_t i = 0; i < app->event_count; i++) {
        if(seen[i]) continue;
        if(strcmp(app->events[i].subsystem, "NFC") != 0) continue;
        uint8_t a[16];
        int a_len = hex_to_bytes(app->events[i].identifier, a, sizeof(a));
        if(a_len < 4) continue;

        uint32_t cluster = 1;
        for(size_t j = i + 1; j < app->event_count; j++) {
            if(seen[j]) continue;
            if(strcmp(app->events[j].subsystem, "NFC") != 0) continue;
            uint8_t b[16];
            int b_len = hex_to_bytes(app->events[j].identifier, b, sizeof(b));
            if(b_len < 4) continue;
            if(memcmp(a, b, 4) == 0) {
                cluster++;
                seen[j] = true;
            }
        }
        if(cluster >= 3) {
            groups_found++;
            if(groups_found == 1) {
                snprintf(
                    first_prefix_hex,
                    sizeof(first_prefix_hex),
                    "%02X%02X%02X%02X",
                    a[0], a[1], a[2], a[3]);
            }
        }
        seen[i] = true;
    }
    furi_string_cat_printf(
        app->report,
        "[Shared Prefix]\n%lu cluster(s) of >=3\nUIDs sharing 4 bytes\n%s%s\n\n",
        (unsigned long)groups_found,
        groups_found ? "prefix: " : "-> none detected",
        groups_found ? first_prefix_hex : "");
}

/** v0.1 — Most-repeated NFC tag */
static void analyze_repeat_exposure(PatternAnalyzerApp* app) {
    uint32_t max_repeats = 0;
    char     top_uid[PATTERN_ANALYZER_UID_LEN] = {0};
    bool     counted[PATTERN_ANALYZER_MAX_EVENTS];
    memset(counted, 0, sizeof(counted));

    for(size_t i = 0; i < app->event_count; i++) {
        if(counted[i]) continue;
        if(strcmp(app->events[i].subsystem, "NFC") != 0) continue;

        uint32_t cnt = 1;
        for(size_t j = i + 1; j < app->event_count; j++) {
            if(counted[j]) continue;
            if(strcmp(app->events[j].identifier, app->events[i].identifier) == 0) {
                cnt++;
                counted[j] = true;
            }
        }
        if(cnt > max_repeats) {
            max_repeats = cnt;
            strlcpy(top_uid, app->events[i].identifier, sizeof(top_uid));
        }
        counted[i] = true;
    }
    furi_string_cat_printf(
        app->report,
        "[Repeat Exposure]\ntop NFC tag seen %lu x\n%s\n%s\n\n",
        (unsigned long)max_repeats,
        max_repeats ? top_uid : "(no NFC events)",
        max_repeats > 5 ? "-> frequently used tag" : "-> normal frequency");
}

/** v0.1 — Peak events in any 60-second window */
static void analyze_burst(PatternAnalyzerApp* app) {
    uint32_t max_in_window = 0;
    uint32_t burst_start   = 0;

    for(size_t i = 0; i < app->event_count; i++) {
        if(app->events[i].epoch_seconds == 0) continue;
        uint32_t start = app->events[i].epoch_seconds;
        uint32_t count = 0;
        for(size_t j = i; j < app->event_count; j++) {
            if(app->events[j].epoch_seconds == 0) continue;
            if(app->events[j].epoch_seconds >= start &&
               app->events[j].epoch_seconds < start + 60) {
                count++;
            }
        }
        if(count > max_in_window) {
            max_in_window = count;
            burst_start   = start;
        }
    }
    if(max_in_window > 20) {
        DateTime bdt;
        datetime_timestamp_to_datetime(burst_start, &bdt);
        furi_string_cat_printf(
            app->report,
            "[Burst 60s]\n%lu events in 60s at\n%02u:%02u:%02u\n-> active scan!\n\n",
            (unsigned long)max_in_window,
            bdt.hour, bdt.minute, bdt.second);
    } else {
        furi_string_cat_printf(
            app->report,
            "[Burst 60s]\npeak %lu events/60s\n-> normal activity\n\n",
            (unsigned long)max_in_window);
    }
}

/* ── NEW v0.2 analyses ────────────────────────────────────────────────── */

/**
 * v0.2 — Short-burst: peak events in any 5-second window.
 * Signals rapid scanning when >=3 events cluster tightly.
 */
static void analyze_short_burst(PatternAnalyzerApp* app) {
    uint32_t max_in_5s = 0;
    uint32_t burst_epoch = 0;

    for(size_t i = 0; i < app->event_count; i++) {
        if(app->events[i].epoch_seconds == 0) continue;
        uint32_t start = app->events[i].epoch_seconds;
        uint32_t count = 0;
        for(size_t j = i; j < app->event_count; j++) {
            if(app->events[j].epoch_seconds == 0) continue;
            if(app->events[j].epoch_seconds >= start &&
               app->events[j].epoch_seconds < start + 5) {
                count++;
            }
        }
        if(count > max_in_5s) {
            max_in_5s   = count;
            burst_epoch = start;
        }
    }

    if(max_in_5s >= 3) {
        DateTime bdt;
        datetime_timestamp_to_datetime(burst_epoch, &bdt);
        furi_string_cat_printf(
            app->report,
            "[Short Burst <5s]\n%lu events in 5s at\n%02u:%02u:%02u\n-> rapid scan!\n\n",
            (unsigned long)max_in_5s,
            bdt.hour, bdt.minute, bdt.second);
    } else {
        furi_string_cat_printf(
            app->report,
            "[Short Burst <5s]\npeak %lu events/5s\n-> normal pace\n\n",
            (unsigned long)max_in_5s);
    }
}

/**
 * v0.2 — Cross-protocol correlation: same identifier seen in events from
 * two different subsystems (e.g., same value in NFC and SubGhz logs).
 * Uses O(n²) scan — acceptable for <=200 events.
 */
static void analyze_cross_protocol(PatternAnalyzerApp* app) {
    uint32_t cross_hits = 0;
    char     first_id[PATTERN_ANALYZER_UID_LEN]    = {0};
    char     first_sys1[PATTERN_ANALYZER_FIELD_LEN] = {0};
    char     first_sys2[PATTERN_ANALYZER_FIELD_LEN] = {0};

    for(size_t i = 0; i < app->event_count; i++) {
        if(strlen(app->events[i].identifier) < 4) continue;
        for(size_t j = i + 1; j < app->event_count; j++) {
            if(strcmp(app->events[i].subsystem, app->events[j].subsystem) == 0) continue;
            if(strcmp(app->events[i].identifier, app->events[j].identifier) != 0) continue;
            cross_hits++;
            if(cross_hits == 1) {
                strlcpy(first_id,   app->events[i].identifier, sizeof(first_id));
                strlcpy(first_sys1, app->events[i].subsystem,  sizeof(first_sys1));
                strlcpy(first_sys2, app->events[j].subsystem,  sizeof(first_sys2));
            }
        }
    }

    furi_string_cat_printf(
        app->report,
        "[Cross-Protocol]\n%lu match(es) across\nmultiple subsystems\n",
        (unsigned long)cross_hits);
    if(cross_hits) {
        furi_string_cat_printf(
            app->report,
            "%.16s\n(%s + %s)\n\n",
            first_id, first_sys1, first_sys2);
    } else {
        furi_string_cat_str(app->report, "-> none detected\n\n");
    }
}

/**
 * v0.2 — Top-identifier frequency: rank all identifiers across all subsystems.
 * Shows the two most-repeated identifiers and their hit counts.
 * Uses heap-allocated arrays to keep stack usage low.
 */
static void analyze_top_identifiers(PatternAnalyzerApp* app) {
    furi_string_cat_str(app->report, "[Top Identifiers]\n");
    if(app->event_count == 0) {
        furi_string_cat_str(app->report, "no data\n\n");
        return;
    }

    uint32_t* counts = malloc(sizeof(uint32_t) * app->event_count);
    bool*     skip   = malloc(sizeof(bool) * app->event_count);
    if(!counts || !skip) {
        free(counts);
        free(skip);
        furi_string_cat_str(app->report, "alloc failed\n\n");
        return;
    }
    memset(counts, 0, sizeof(uint32_t) * app->event_count);
    memset(skip,   0, sizeof(bool) * app->event_count);

    /* Count each unique identifier */
    for(size_t i = 0; i < app->event_count; i++) {
        if(skip[i]) continue;
        if(strlen(app->events[i].identifier) < 2) { skip[i] = true; continue; }
        counts[i] = 1;
        for(size_t j = i + 1; j < app->event_count; j++) {
            if(skip[j]) continue;
            if(strcmp(app->events[i].identifier, app->events[j].identifier) == 0) {
                counts[i]++;
                skip[j] = true;
            }
        }
    }

    /* Find top-2 */
    size_t top1 = app->event_count; /* sentinel = none */
    size_t top2 = app->event_count;
    for(size_t i = 0; i < app->event_count; i++) {
        if(counts[i] == 0) continue;
        if(top1 == app->event_count || counts[i] > counts[top1]) {
            top2 = top1;
            top1 = i;
        } else if(top2 == app->event_count || counts[i] > counts[top2]) {
            top2 = i;
        }
    }

    if(top1 < app->event_count) {
        furi_string_cat_printf(
            app->report,
            "#1 (%lux) %.18s\n",
            (unsigned long)counts[top1],
            app->events[top1].identifier);
    }
    if(top2 < app->event_count) {
        furi_string_cat_printf(
            app->report,
            "#2 (%lux) %.18s\n",
            (unsigned long)counts[top2],
            app->events[top2].identifier);
    }
    if(top1 == app->event_count) {
        furi_string_cat_str(app->report, "IDs too short\n");
    }

    free(counts);
    free(skip);
    furi_string_cat_str(app->report, "\n");
}

/* ── NEW v0.3 analyses ────────────────────────────────────────────────── */

/**
 * v0.3 — Relay attack detection.
 *
 * A relay attack on NFC/LFRFID works as follows:
 *   Attacker A reads victim's card (within 5 cm) while Attacker B stands
 *   near the target reader and relays the credential over SubGHz RF.
 *
 * Signature in the audit log: a credential read (NFC or LFRFID) is followed
 * within RELAY_WINDOW_S seconds by a SubGHz event or persistent RF source.
 *
 * Inspired by:
 *   - DEF CON 32 "Mutual Authentication is Optional" (iClass research)
 *   - bettse/seader: credential extraction via SAM board relay
 *   - Flipper NFC relay research at hackyourmom.com
 */
#define RELAY_WINDOW_S 30u

static void analyze_relay_attack(PatternAnalyzerApp* app) {
    uint32_t relay_hits = 0;
    char     first_ts[9] = "--:--:--";

    for(size_t i = 0; i < app->event_count; i++) {
        /* Source: credential read events */
        const char* sub = app->events[i].subsystem;
        bool is_cred = (strcmp(sub, "NFC") == 0 || strcmp(sub, "LFRFID") == 0);
        if(!is_cred) continue;
        if(app->events[i].epoch_seconds == 0) continue;

        uint32_t t0 = app->events[i].epoch_seconds;

        /* Look for SubGHz activity within the relay window */
        for(size_t j = i + 1; j < app->event_count; j++) {
            uint32_t tj = app->events[j].epoch_seconds;
            if(tj == 0) continue;
            if(tj < t0) continue;
            if(tj > t0 + RELAY_WINDOW_S) break;

            const char* js = app->events[j].subsystem;
            const char* jo = app->events[j].operation;
            bool is_rf = (strcmp(js, "SubGhz") == 0 || strcmp(js, "SubGHz") == 0);
            bool is_persist = (is_rf && strcmp(jo, "PERSIST") == 0);
            if(is_rf || is_persist) {
                relay_hits++;
                if(relay_hits == 1) {
                    DateTime dt;
                    datetime_timestamp_to_datetime(t0, &dt);
                    snprintf(first_ts, sizeof(first_ts),
                        "%02u:%02u:%02u", dt.hour, dt.minute, dt.second);
                }
                break; /* one match per credential event is enough */
            }
        }
    }

    if(relay_hits > 0) {
        furi_string_cat_printf(
            app->report,
            "[Relay Attack?]\n"
            "%lu cred+RF pair(s)\nwithin %us window\n"
            "First at %s\n"
            "-> INVESTIGATE\n\n",
            (unsigned long)relay_hits,
            (unsigned)RELAY_WINDOW_S,
            first_ts);
    } else {
        furi_string_cat_str(
            app->report,
            "[Relay Attack?]\nNo cred+RF pairing\ndetected\n-> clear\n\n");
    }
}

/**
 * v0.3 — Night activity detection.
 *
 * Events between 00:00 and 05:00 are unusual for most legitimate use cases
 * and may indicate unattended device usage, scheduled attacks, or compromise.
 */
static void analyze_night_activity(PatternAnalyzerApp* app) {
    uint32_t night_count = 0;
    char     first_ts[9] = "--:--:--";

    for(size_t i = 0; i < app->event_count; i++) {
        if(app->events[i].epoch_seconds == 0) continue;
        DateTime dt;
        datetime_timestamp_to_datetime(app->events[i].epoch_seconds, &dt);
        /* 00:00 – 04:59 inclusive */
        if(dt.hour < 5u) {
            night_count++;
            if(night_count == 1) {
                snprintf(first_ts, sizeof(first_ts),
                    "%02u:%02u:%02u", dt.hour, dt.minute, dt.second);
            }
        }
    }

    if(night_count > 0) {
        furi_string_cat_printf(
            app->report,
            "[Night Activity]\n%lu event(s) 00-05h\nEarliest: %s\n"
            "-> unusual hours!\n\n",
            (unsigned long)night_count,
            first_ts);
    } else {
        furi_string_cat_str(
            app->report,
            "[Night Activity]\nNo events 00-05h\n-> normal schedule\n\n");
    }
}

/**
 * v0.3 — Threat diversity / attack surface profile.
 *
 * Inspired by Kashmir54's VoyagerRF multi-vector RF research and the
 * iClass credential research showing multi-stage attack chains.
 *
 * Scores how many of the 7 known attack vectors were used today.
 * A high diversity score (≥4) suggests a systematic security assessment
 * or a complex multi-stage attack against different layers.
 */
static void analyze_threat_profile(PatternAnalyzerApp* app) {
    /*
     * Vector names match subsystem strings in the audit CSV.
     * BadKB is counted as an OFFENSIVE vector only when op == "RUN".
     * BadKB/AUDIT (Sentinel defensive payloads) are counted separately
     * in `sentinel_count` so the threat verdict isn't inflated by
     * defensive activity.
     */
    static const char* const VECTORS[] = {
        "NFC", "SubGhz", "IR", "BadKB", "LFRFID", "iButton", "PERSIST"
    };
    bool     seen[7];
    uint16_t sentinel_count = 0;
    memset(seen, 0, sizeof(seen));

    for(size_t i = 0; i < app->event_count; i++) {
        const char* s = app->events[i].subsystem;
        const char* o = app->events[i].operation;

        /* Sentinel defensive activity — does NOT count toward offensive vectors */
        if(strcmp(s, "BadKB") == 0 &&
           (strcmp(o, "AUDIT") == 0 || strcmp(o, "PREVIEW") == 0)) {
            sentinel_count++;
            continue;
        }

        for(int k = 0; k < 6; k++) {
            if(strcmp(s, VECTORS[k]) == 0) { seen[k] = true; break; }
        }
        /* SubGhz/PERSIST is tracked as vector 6 */
        if((strcmp(s, "SubGhz") == 0 || strcmp(s, "SubGHz") == 0) &&
           strcmp(o, "PERSIST") == 0) {
            seen[6] = true;
        }
    }

    uint8_t diversity = 0;
    for(int k = 0; k < 7; k++) if(seen[k]) diversity++;

    furi_string_cat_str(app->report, "[Threat Profile]\n");
    furi_string_cat_printf(app->report, "Vectors: %u/7\n", (unsigned)diversity);
    for(int k = 0; k < 7; k++) {
        if(seen[k]) furi_string_cat_printf(app->report, " + %s\n", VECTORS[k]);
    }

    const char* verdict =
        (diversity >= 5) ? "-> FULL spectrum op\n" :
        (diversity >= 3) ? "-> multi-vector use\n" :
        (diversity >= 2) ? "-> dual-vector\n"      :
                           "-> focused activity\n";
    furi_string_cat_str(app->report, verdict);

    /* Show defensive activity as separate metric */
    if(sentinel_count > 0) {
        furi_string_cat_printf(
            app->report,
            "Defensive: %u Sentinel\n",
            (unsigned)sentinel_count);
    }
    furi_string_cat_str(app->report, "\n");
}

/* ── public API ──────────────────────────────────────────────────────── */

void pattern_analyzer_run_analysis(PatternAnalyzerApp* app) {
    furi_string_reset(app->report);
    size_t n = pa_load_today(app);

    furi_string_cat_printf(
        app->report,
        "Loaded %u events\nfrom today's log\n\n",
        (unsigned)n);

    if(n == 0) {
        furi_string_cat_str(
            app->report,
            "No data to analyze.\n"
            "Enable audit and use\n"
            "NFC/SubGhz/IR first.");
        return;
    }

    /* ── v0.1 algorithms ── */
    analyze_sequential_uids(app);
    analyze_shared_prefix(app);
    analyze_repeat_exposure(app);
    analyze_burst(app);
    /* ── v0.2 algorithms ── */
    analyze_short_burst(app);
    analyze_cross_protocol(app);
    analyze_top_identifiers(app);
    /* ── v0.3 algorithms (community research integration) ── */
    analyze_relay_attack(app);     /* iClass/NFC relay research */
    analyze_night_activity(app);   /* unusual schedule detection */
    analyze_threat_profile(app);   /* multi-vector fingerprint   */

    furi_string_cat_str(app->report, "(scope: TODAY)\nBack to return.");
}

bool pattern_analyzer_export_report(PatternAnalyzerApp* app) {
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);

    FuriString* path = furi_string_alloc_printf(
        PATTERN_ANALYZER_AUDIT_DIR "/report-%04u-%02u-%02u.txt",
        (unsigned)dt.year,
        (unsigned)dt.month,
        (unsigned)dt.day);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File*    file    = storage_file_alloc(storage);
    bool     ok      = false;

    if(storage_file_open(file, furi_string_get_cstr(path), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        const char* text = furi_string_get_cstr(app->report);
        uint32_t    len  = (uint32_t)strlen(text);
        ok = (storage_file_write(file, text, len) == len);
        storage_file_close(file);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    furi_string_free(path);
    return ok;
}
