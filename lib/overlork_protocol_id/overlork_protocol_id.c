/**
 * @file overlork_protocol_id.c
 * @brief RF protocol identification — v2, "te"-based algorithm.
 *
 * v2 changes vs v1 (histogram approach):
 * ──────────────────────────────────────
 * ProtoView (github.com/antirez/protoview) popularised the "te" concept:
 * virtually all OOK encodings are built from multiples of a single chip
 * duration. Rather than binning into a fixed histogram, v2 locates te by
 * finding the minimum non-noise pulse, then clusters all pulses into
 * 1te / 2te / 3te buckets with ±40% tolerance.
 *
 * Benefits over the v1 histogram:
 *   • No bin-size tuning — works for any chip time from ~50 µs to ~2 ms.
 *   • Handles short captures (≥10 pulses) better.
 *   • More robust against noise spikes that inflate histogram bins.
 *   • TPMS sensor detection added: long Manchester preamble + 48+ bits.
 *
 * No dynamic allocation; all state fits in local variables on the stack.
 *
 * Technique credit: ProtoView by Salvatore Sanfilippo (@antirez)
 * Author : Eudys Ramirez (@OverL0rk)
 */

#include "overlork_protocol_id.h"

/* ── constants ──────────────────────────────────────────────────────────── */

/** Minimum pulse width considered signal (not noise floor glitch), µs */
#define OL_TE_MIN_VALID_US   50u
/** Maximum pulse width considered a chip (not a preamble sync gap), µs */
#define OL_TE_MAX_CHIP_US  5000u
/** Tolerance for pulse-cluster membership: ±OL_TE_TOL_PCT % of te×N */
#define OL_TE_TOL_PCT        40u
/** NEC 9 ms leader burst threshold, µs */
#define OL_NEC_LEADER_MIN_US 7000u
/** TPMS minimum estimated bit count (64–128 bits for common sensors) */
#define OL_TPMS_MIN_BITS     48u
/** TPMS chip time window (Renault/Toyota/Ford: 64–520 µs) */
#define OL_TPMS_TE_MIN_US    60u
#define OL_TPMS_TE_MAX_US   520u

/* ── internal helpers ───────────────────────────────────────────────────── */

/** True if @p dur is within ±OL_TE_TOL_PCT % of @p target. */
static bool te_fits(uint32_t dur, uint32_t target) {
    if(target == 0) return false;
    uint32_t lo = (target * (100u - OL_TE_TOL_PCT)) / 100u;
    uint32_t hi = (target * (100u + OL_TE_TOL_PCT)) / 100u;
    return (dur >= lo) && (dur <= hi);
}

/**
 * Find the chip time "te" using a 8-slot sorted min-window.
 *
 * We collect the 8 shortest valid pulses and return their median.
 * Using the median (not the absolute minimum) rejects rare noise glitches
 * that would otherwise make te too small and break the bucket classifier.
 *
 * ProtoView uses a similar approach: the shortest "sensible" pulse is 1te.
 *
 * @return te in µs, or 0 if no valid pulses exist.
 */
static uint32_t find_te(const uint32_t* dur, uint16_t count) {
    uint32_t win[8];
    uint8_t  n = 0;

    for(uint16_t i = 0; i < count; i++) {
        if(dur[i] < OL_TE_MIN_VALID_US) continue;
        if(dur[i] > OL_TE_MAX_CHIP_US)  continue;

        /* Maintain a sorted ascending window of size 8 */
        if(n < 8u) {
            win[n++] = dur[i];
            /* Insertion sort (tiny n, effectively O(1)) */
            for(uint8_t k = n - 1; k > 0 && win[k] < win[k - 1]; k--) {
                uint32_t t = win[k]; win[k] = win[k-1]; win[k-1] = t;
            }
        } else if(dur[i] < win[7]) {
            win[7] = dur[i];
            for(uint8_t k = 7; k > 0 && win[k] < win[k - 1]; k--) {
                uint32_t t = win[k]; win[k] = win[k-1]; win[k-1] = t;
            }
        }
    }

    return (n == 0) ? 0u : win[n / 2u]; /* median of bottom-n */
}

/**
 * Bucket pulses into 1te / 2te / 3te / other categories.
 */
static void classify_buckets(
    const uint32_t* dur, uint16_t count, uint32_t te,
    uint16_t* n1, uint16_t* n2, uint16_t* n3, uint16_t* n_other)
{
    *n1 = *n2 = *n3 = *n_other = 0;
    for(uint16_t i = 0; i < count; i++) {
        if(dur[i] < OL_TE_MIN_VALID_US) continue;
        if     (te_fits(dur[i], te))        (*n1)++;
        else if(te_fits(dur[i], te * 2u))   (*n2)++;
        else if(te_fits(dur[i], te * 3u))   (*n3)++;
        else                                (*n_other)++;
    }
}

/**
 * TPMS fingerprint: Manchester preamble of ≥16 consecutive te/2te pulses
 * at a chip time in the TPMS range, with ≥48 estimated bits.
 *
 * Inspired by ProtoView's TPMS decoder which identifies sensor families
 * (Renault, Toyota, Schrader, Citroën, Ford) by their preamble length.
 */
static bool looks_like_tpms(
    const uint32_t* dur, uint16_t count, uint32_t te, uint16_t bit_count)
{
    if(te < OL_TPMS_TE_MIN_US || te > OL_TPMS_TE_MAX_US) return false;
    if(bit_count < OL_TPMS_MIN_BITS)                       return false;

    /* Count leading alternating 1te/2te pulses = preamble */
    uint8_t preamble = 0;
    for(uint16_t i = 0; i < count && preamble < 32u; i++) {
        if(te_fits(dur[i], te) || te_fits(dur[i], te * 2u))
            preamble++;
        else
            break;
    }
    return (preamble >= 16u);
}

/**
 * Estimate packet repetition count.
 * Gaps ≥ 10×te are inter-packet pauses in most OOK protocols.
 */
static uint8_t count_repeats(const uint32_t* dur, uint16_t count, uint32_t te) {
    if(te == 0) return 1u;
    uint8_t  reps      = 1u;
    uint32_t gap_thr   = te * 10u;
    for(uint16_t i = 0; i < count && reps < 8u; i++) {
        if(dur[i] > gap_thr) reps++;
    }
    return reps;
}

/* ── public API ─────────────────────────────────────────────────────────── */

bool ol_protocol_id(
    const uint32_t* durations_us,
    uint16_t        count,
    OlProtoResult*  result)
{
    if(!durations_us || !result) return false;
    if(count < OL_PROTO_MIN_PULSES) return false;
    if(count > OL_PROTO_MAX_PULSES) count = OL_PROTO_MAX_PULSES;

    result->type         = OlProtoUnknown;
    result->confidence   = 0;
    result->chip_time_us = 0;
    result->bit_count    = 0;
    result->repeat_count = 1;

    /* ── 1. NEC fast-path: 9 ms leader pulse ──────────────────────────── */
    if(durations_us[0] >= OL_NEC_LEADER_MIN_US) {
        result->type         = OlProtoNec;
        result->confidence   = 85;
        result->chip_time_us = 562u; /* NEC: 562.5 µs chip */
        result->bit_count    = 32;
        result->repeat_count = count_repeats(durations_us, count, 562u);
        return true;
    }

    /* ── 2. Find chip time te ─────────────────────────────────────────── */
    uint32_t te = find_te(durations_us, count);
    if(te == 0) {
        result->type       = OlProtoRaw;
        result->confidence = 15;
        return false;
    }
    result->chip_time_us = te;

    /* ── 3. Bucket all pulses ─────────────────────────────────────────── */
    uint16_t n1, n2, n3, n_other;
    classify_buckets(durations_us, count, te, &n1, &n2, &n3, &n_other);

    uint16_t total_valid = n1 + n2 + n3 + n_other;
    if(total_valid == 0) {
        result->type       = OlProtoRaw;
        result->confidence = 10;
        return false;
    }

    uint16_t n_fit   = n1 + n2 + n3;
    uint8_t  fit_pct = (uint8_t)((n_fit * 100u) / total_valid);

    /* Bit count estimate: 1te≈1bit, 2te≈0.5bit, 3te≈0.33bit */
    uint16_t bit_est = n1 + (n2 / 2u) + (n3 / 3u);
    result->bit_count    = bit_est;
    result->repeat_count = count_repeats(durations_us, count, te);

    /* ── 4. TPMS fingerprint ─────────────────────────────────────────── */
    if(looks_like_tpms(durations_us, count, te, bit_est)) {
        result->type       = OlProtoTpms;
        result->confidence = (uint8_t)(50u + (fit_pct / 4u));
        if(result->confidence > 95u) result->confidence = 95u;
        return (result->confidence >= 40u);
    }

    /* ── 5. PWM vs Manchester ────────────────────────────────────────── */
    /*
     * Key insight from ProtoView: in Manchester encoding every bit produces
     * exactly one mid-bit transition, leading to a high proportion of 2te
     * pulses (clock period = 2te).  PWM-OOK has mostly 1te gaps with
     * variable-width data pulses (1te short, 3te long).
     *
     * Threshold: if >25% of all valid pulses are 2te → Manchester.
     */
    bool is_manchester = (n2 > (total_valid / 4u));

    if(fit_pct < 40u) {
        result->type       = OlProtoRaw;
        result->confidence = fit_pct / 2u;
        return false;
    }

    result->type       = is_manchester ? OlProtoManchesterOok : OlProtoPwmOok;
    result->confidence = (uint8_t)(40u + (fit_pct * 60u) / 100u);
    if(result->confidence > 100u) result->confidence = 100u;

    return (result->confidence >= 40u);
}

const char* ol_proto_type_str(OlProtoType type) {
    switch(type) {
    case OlProtoUnknown:       return "Unknown";
    case OlProtoPwmOok:        return "PWM-OOK";
    case OlProtoManchesterOok: return "Manchester";
    case OlProtoNec:           return "NEC";
    case OlProtoTpms:          return "TPMS";
    case OlProtoRaw:           return "Raw";
    default:                   return "?";
    }
}
