/**
 * @file sentinel_payloads.h
 * @brief OverL0rk Sentinel — defensive BadUSB payload registry.
 *
 * Each payload is a read-only audit ducky script that gathers information
 * from the host system (open ports, AV status, processes, hashes) and
 * writes the output to a local file the user can review.
 *
 * IMPORTANT — these are NOT offensive payloads:
 *   - Zero modifications to the host
 *   - All output written to user-readable temp files (C:\Temp\ or ~/)
 *   - All commands are documented and require no elevation by default
 *
 * Detection by the BadKB hook:
 *   Any path containing "Sentinel" triggers operation="AUDIT" (not "RUN")
 *   → flipper_guard treats it as Info, not Critical
 *   → pattern_analyzer flags it under threat_profile's defensive vector
 */

#pragma once

#include <stdint.h>
#include <stddef.h>

/** A single Sentinel payload entry */
typedef struct {
    const char* id;           /**< Filename (no extension): "win_portscan" */
    const char* label;        /**< Menu label: "Win: Port Scan"             */
    const char* description;  /**< 3-line ~16ch-wide widget description     */
    const char* ducky;        /**< Full ducky-script content                */
} SentinelPayload;

/** Table of all payloads.  Iterate via sentinel_payloads_get(). */
const SentinelPayload* sentinel_payloads_get(size_t* out_count);
