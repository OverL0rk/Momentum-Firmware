/**
 * @file overlork_events.h
 * @brief Typed audit-event model parsed from the OverL0rk audit CSV.
 *
 * Audit CSV format (5 fields):
 *   YYYY-MM-DD HH:MM:SS,subsystem,operation,identifier,details
 *
 * Usage:
 *   char line[256];
 *   // ... fill line from file read ...
 *   OlEvent ev;
 *   if(ol_event_from_csv_line(line, &ev)) {
 *       // use ev.type, ev.subsystem, etc.
 *   }
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Semantic event type derived from the "subsystem" and "operation" fields */
typedef enum {
    OlEventTypeNfcRead,   /**< NFC subsystem, any operation (typically READ)  */
    OlEventTypeSubGhzRx,  /**< SubGhz subsystem RX event                      */
    OlEventTypeIrTx,      /**< Infrared transmission                           */
    OlEventTypeIrRx,      /**< Infrared reception                              */
    OlEventTypeBadKbRun,  /**< BadKB/RUN — offensive ducky script execution   */
    OlEventTypeBadKbAudit, /**< BadKB/AUDIT — defensive Sentinel ducky payload */
    OlEventTypeLfRfidRead, /**< Low-Frequency RFID card read (EM4100, HID …)  */
    OlEventTypeIButtonRead, /**< iButton / 1-Wire key read (Dallas DS199x …)  */
    OlEventTypeSignalPersistent, /**< SubGhz/PERSIST: same source 3+ sweeps  */
    OlEventTypeOther,     /**< Any unrecognised subsystem/operation            */
} OlEventType;

/** Parsed representation of one row from the audit CSV */
typedef struct {
    OlEventType type;         /**< Derived semantic type */
    char subsystem[16];       /**< Raw subsystem string ("NFC", "SubGhz", …) */
    char operation[16];       /**< Raw operation string ("READ", "RX", "RUN", …) */
    char identifier[64];      /**< Tag UID, frequency, script path, … */
    char details[64];         /**< Protocol name, interface, … */
    uint32_t epoch_seconds;   /**< UNIX timestamp, 0 if parse failed */
} OlEvent;

/**
 * Parse one line of the audit CSV into @p out.
 *
 * The line is modified in-place (NUL bytes are written at field boundaries).
 * Header lines ("timestamp,…") are silently rejected and return false.
 *
 * @param line  Mutable NUL-terminated CSV line.
 * @param out   Caller-allocated output struct.
 * @return      true on success; false if the line is a header or malformed.
 */
bool ol_event_from_csv_line(char* line, OlEvent* out);

#ifdef __cplusplus
}
#endif
