/**
 * @file overlork_protocol_id.h
 * @brief Passive RF protocol identification from raw pulse timings.
 *
 * Takes an array of pulse/gap durations (in microseconds) captured from
 * the CC1101 async RX path and classifies the encoding scheme used.
 *
 * Input contract
 * --------------
 * durations_us[]  — alternating high/low pulse durations, starting with
 *                   the first high-going edge.  Values are in microseconds.
 *                   Typical range: 50–10 000 µs per element.
 * count           — number of elements (must be ≥ OL_PROTO_MIN_PULSES and
 *                   ≤ OL_PROTO_MAX_PULSES).
 *
 * No dynamic allocation is used; all analysis runs on the caller's stack.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Minimum number of pulse/gap durations required for classification */
#define OL_PROTO_MIN_PULSES  10u
/** Maximum number of pulse/gap durations accepted (stack budget) */
#define OL_PROTO_MAX_PULSES 512u

/**
 * Protocol encoding family detected.
 *
 * Ordering matches rough "complexity" — Unknown is always 0, Raw is the
 * catch-all. TPMS is added as a named type because its Manchester preamble
 * is distinct enough to fingerprint without full frame decoding.
 */
typedef enum {
    OlProtoUnknown = 0,  /**< Could not classify                            */
    OlProtoPwmOok,       /**< OOK PWM: constant gap, varying pulse width     */
    OlProtoManchesterOok,/**< Manchester-encoded OOK (mid-period transition) */
    OlProtoNec,          /**< NEC-family: 9 ms leader + 4.5 ms space + PWM  */
    OlProtoTpms,         /**< TPMS tire-pressure sensor (Manchester, 64+ b) */
    OlProtoRaw,          /**< No clear structure / long irregular capture    */
} OlProtoType;

/**
 * Result of one protocol-identification pass.
 */
typedef struct {
    OlProtoType type;           /**< Best-fit encoding family             */
    uint8_t     confidence;     /**< Confidence 0–100 (100 = perfect fit) */
    uint32_t    chip_time_us;   /**< Estimated basic symbol duration (µs) */
    uint16_t    bit_count;      /**< Estimated number of data bits        */
    uint8_t     repeat_count;   /**< Number of detected packet repetitions*/
} OlProtoResult;

/**
 * Analyse an array of alternating pulse/gap durations and classify the
 * protocol encoding.
 *
 * @param[in]  durations_us  Array of pulse/gap durations in µs.
 * @param[in]  count         Number of elements (clamped to MAX_PULSES).
 * @param[out] result        Populated on successful classification.
 *
 * @return true  if a definite classification was made (confidence ≥ 40).
 * @return false if the capture is too short, all-noise, or unknown.
 */
bool ol_protocol_id(
    const uint32_t* durations_us,
    uint16_t        count,
    OlProtoResult*  result);

/**
 * Convert an OlProtoType to a short human-readable string.
 *
 * @param type  Protocol type enum value.
 * @return      Pointer to a static string (never NULL).
 */
const char* ol_proto_type_str(OlProtoType type);

#ifdef __cplusplus
}
#endif
