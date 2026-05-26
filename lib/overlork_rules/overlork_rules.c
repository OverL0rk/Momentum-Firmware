#include "overlork_rules.h"
#include <string.h>
#include <stddef.h>

/* ── built-in rule table ─────────────────────────────────────────────── */
/*
 * Rules are evaluated in order; the last matching rule wins (i.e., the
 * catch-all at the bottom is overridden by any more-specific rule above).
 * We walk the table from index 0 and track the highest-severity match.
 */

static const OlRule g_rules[] = {
    /* ── catch-all: any event at least gets Info ─────────────────────── */
    {
        .name       = "Any event",
        .event_type = OlEventTypeOther, /* means: any type */
        .subsystem  = NULL,
        .operation  = NULL,
        .action     = OlRuleActionInfo,
    },
    /* ── NFC ──────────────────────────────────────────────────────────── */
    {
        .name       = "NFC read",
        .event_type = OlEventTypeNfcRead,
        .subsystem  = "NFC",
        .operation  = NULL,
        .action     = OlRuleActionWarn,
    },
    /* ── SubGhz ───────────────────────────────────────────────────────── */
    {
        .name       = "SubGhz RX",
        .event_type = OlEventTypeSubGhzRx,
        .subsystem  = NULL,
        .operation  = NULL,
        .action     = OlRuleActionAlert,
    },
    /* ── IR ───────────────────────────────────────────────────────────── */
    {
        .name       = "IR transmission",
        .event_type = OlEventTypeIrTx,
        .subsystem  = "IR",
        .operation  = "TX",
        .action     = OlRuleActionWarn,
    },
    /* ── BadKB (offensive) ───────────────────────────────────────────── */
    {
        /*
         * Generic BadKB/RUN: an unknown ducky script was executed.
         * Treat as Critical — could be credential exfil, reverse shell,
         * persistence install.  Distinguished from Sentinel AUDIT scripts
         * (next rule) by the BadKb hook which sets op="AUDIT" when the
         * script path contains "Sentinel".
         */
        .name       = "BadKB script run",
        .event_type = OlEventTypeBadKbRun,
        .subsystem  = "BadKB",
        .operation  = NULL,
        .action     = OlRuleActionCritical,
    },
    /* ── BadKB (defensive Sentinel) ──────────────────────────────────── */
    {
        /*
         * BadKB/AUDIT (Sentinel script) or BadKB/PREVIEW (user opened the
         * detail view).  Both are defensive operations and only deserve
         * Info severity — no LED flash, no log spam.  The audit trail still
         * exists so pattern_analyzer can correlate Sentinel usage over time.
         */
        .name       = "Sentinel audit payload",
        .event_type = OlEventTypeBadKbAudit,
        .subsystem  = "BadKB",
        .operation  = NULL,
        .action     = OlRuleActionInfo,
    },
    /* ── LF RFID ─────────────────────────────────────────────────────── */
    {
        .name       = "LFRFID card read",
        .event_type = OlEventTypeLfRfidRead,
        .subsystem  = "LFRFID",
        .operation  = NULL,
        .action     = OlRuleActionWarn,   /* same tier as NFC — access token risk */
    },
    /* ── iButton ─────────────────────────────────────────────────────── */
    {
        .name       = "iButton key read",
        .event_type = OlEventTypeIButtonRead,
        .subsystem  = "iButton",
        .operation  = NULL,
        .action     = OlRuleActionWarn,   /* door-key / intercom credential */
    },
    /* ── Persistent RF source ────────────────────────────────────────── */
    {
        /*
         * A signal detected in 3+ consecutive rf_scanner sweeps indicates
         * a stationary transmitter: could be a tracking device, ghost
         * repeater, or surveillance hardware.  Alert (not Critical) because
         * there are legitimate persistent sources (IoT sensors, weather
         * stations) — operator must judge context.
         */
        .name       = "Persistent RF source",
        .event_type = OlEventTypeSignalPersistent,
        .subsystem  = "SubGhz",
        .operation  = "PERSIST",
        .action     = OlRuleActionAlert,
    },
};

static const size_t g_rule_count = sizeof(g_rules) / sizeof(g_rules[0]);

/* ── matching logic ──────────────────────────────────────────────────── */

/**
 * Return true if @p rule matches @p event.
 *
 * A NULL constraint field means "any value".
 * OlEventTypeOther in the rule means "any event type".
 */
static bool rule_matches(const OlRule* rule, const OlEvent* event) {
    /* Type constraint: OlEventTypeOther is the wildcard */
    if(rule->event_type != OlEventTypeOther &&
       rule->event_type != event->type) {
        return false;
    }
    /* Optional subsystem constraint */
    if(rule->subsystem != NULL &&
       strcmp(rule->subsystem, event->subsystem) != 0) {
        return false;
    }
    /* Optional operation constraint */
    if(rule->operation != NULL &&
       strcmp(rule->operation, event->operation) != 0) {
        return false;
    }
    return true;
}

/* ── public API ──────────────────────────────────────────────────────── */

const OlRule* ol_rule_check(const OlEvent* event) {
    if(!event) return NULL;

    const OlRule* best = NULL;

    for(size_t i = 0; i < g_rule_count; i++) {
        if(!rule_matches(&g_rules[i], event)) continue;
        /* Keep the highest-severity match */
        if(best == NULL || g_rules[i].action > best->action) {
            best = &g_rules[i];
        }
    }

    return best;
}

const OlRule* ol_rule_get_table(size_t* count) {
    if(count) *count = g_rule_count;
    return g_rules;
}
