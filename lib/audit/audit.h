#pragma once

#include <stdbool.h>
#include <storage/storage.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AUDIT_BASE_PATH       EXT_PATH("audit")
#define AUDIT_SETTINGS_PATH   EXT_PATH("audit/.audit.settings")
#define AUDIT_MAX_FIELD_LEN   64

/**
 * Log an audit event. No-op if audit is disabled. Thread-safe.
 *
 * The event is appended as one CSV row to /ext/audit/audit-YYYY-MM-DD.csv.
 * Columns: timestamp,subsystem,operation,identifier,details
 *
 * All string parameters are required (use "" for empty). Any field longer than
 * AUDIT_MAX_FIELD_LEN is truncated. Newlines and commas in input are replaced
 * with spaces to keep CSV well-formed.
 *
 * subsystem  e.g. "NFC", "SubGhz", "IR", "BadKB"
 * operation  e.g. "READ", "SAVE", "RUN", "RX", "TX"
 * identifier tag UID hex, frequency, signal name, script path...
 * details    free-form descriptor
 */
void audit_log_event(
    const char* subsystem,
    const char* operation,
    const char* identifier,
    const char* details);

bool audit_is_enabled(void);
void audit_set_enabled(bool enabled);

#ifdef __cplusplus
}
#endif
