#include "overlork_events.h"

#include <datetime/datetime.h>
#include <string.h>
#include <stdlib.h>

/* ── internal helpers ─────────────────────────────────────────────────── */

static uint32_t parse_ts(const char* ts) {
    int y = 0, mo = 0, d = 0, h = 0, mi = 0, s = 0;
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

static OlEventType classify(const char* subsys, const char* op) {
    if(strcmp(subsys, "NFC") == 0)    return OlEventTypeNfcRead;
    if(strcmp(subsys, "SubGhz") == 0 || strcmp(subsys, "SubGHz") == 0) {
        /* Persistent source detection logged as SubGhz/PERSIST */
        if(strcmp(op, "PERSIST") == 0) return OlEventTypeSignalPersistent;
        return OlEventTypeSubGhzRx;
    }
    if(strcmp(subsys, "IR") == 0)
        return (strcmp(op, "TX") == 0) ? OlEventTypeIrTx : OlEventTypeIrRx;
    if(strcmp(subsys, "BadKB") == 0) {
        /* Sentinel defensive payloads log as BadKB/AUDIT (or BadKB/PREVIEW),
         * regular offensive ducky scripts log as BadKB/RUN.
         * Treat both AUDIT and PREVIEW as the defensive type — preview means
         * the user is just viewing the payload info, not running it. */
        if(strcmp(op, "AUDIT") == 0 || strcmp(op, "PREVIEW") == 0)
            return OlEventTypeBadKbAudit;
        return OlEventTypeBadKbRun;
    }
    if(strcmp(subsys, "LFRFID") == 0)  return OlEventTypeLfRfidRead;
    if(strcmp(subsys, "iButton") == 0) return OlEventTypeIButtonRead;
    return OlEventTypeOther;
}

/* ── public API ──────────────────────────────────────────────────────── */

bool ol_event_from_csv_line(char* line, OlEvent* out) {
    if(!line || !out) return false;

    /* Skip CSV header row */
    if(strncmp(line, "timestamp", 9) == 0) return false;

    memset(out, 0, sizeof(OlEvent));

    /* Field 0: timestamp "YYYY-MM-DD HH:MM:SS" */
    char* c1 = strchr(line, ',');
    if(!c1) return false;
    *c1 = '\0';
    out->epoch_seconds = parse_ts(line);

    /* Field 1: subsystem */
    char* f1 = c1 + 1;
    char* c2 = strchr(f1, ',');
    if(!c2) return false;
    *c2 = '\0';
    strlcpy(out->subsystem, f1, sizeof(out->subsystem));

    /* Field 2: operation */
    char* f2 = c2 + 1;
    char* c3 = strchr(f2, ',');
    if(!c3) return false;
    *c3 = '\0';
    strlcpy(out->operation, f2, sizeof(out->operation));

    /* Field 3: identifier */
    char* f3 = c3 + 1;
    char* c4 = strchr(f3, ',');
    if(c4) {
        *c4 = '\0';
        /* Field 4: details (strip trailing newline) */
        char* det = c4 + 1;
        char* nl  = strchr(det, '\n');
        if(nl) *nl = '\0';
        nl = strchr(det, '\r');
        if(nl) *nl = '\0';
        strlcpy(out->details, det, sizeof(out->details));
    }
    strlcpy(out->identifier, f3, sizeof(out->identifier));

    out->type = classify(out->subsystem, out->operation);
    return true;
}
