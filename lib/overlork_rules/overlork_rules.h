/**
 * @file overlork_rules.h
 * @brief Declarative rule engine for OverL0rk security heuristics.
 *
 * Rules map OlEvent types to severity actions. The engine iterates the
 * built-in rule table and returns the highest-severity matching rule.
 *
 * Usage:
 *   OlEvent ev;
 *   ol_event_from_csv_line(line, &ev);
 *   const OlRule* rule = ol_rule_check(&ev);
 *   if(rule) {
 *       ol_notify(notifications, (OlNotifySeverity)rule->action);
 *   }
 */

#pragma once

#include <overlork_events/overlork_events.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Severity level of a rule match.
 * Values are intentionally ordered so higher = more severe.
 */
typedef enum {
    OlRuleActionInfo     = 0, /**< Routine event — cyan LED */
    OlRuleActionWarn     = 1, /**< Noteworthy — yellow LED */
    OlRuleActionAlert    = 2, /**< Suspicious — red LED */
    OlRuleActionCritical = 3, /**< High-risk — magenta + vibro */
} OlRuleAction;

/** A single declarative rule entry */
typedef struct {
    const char*  name;       /**< Human-readable rule name (for logs/UI) */
    OlEventType  event_type; /**< OlEventTypeOther matches ANY type */
    const char*  subsystem;  /**< NULL matches any subsystem */
    const char*  operation;  /**< NULL matches any operation */
    OlRuleAction action;     /**< Severity when this rule fires */
} OlRule;

/**
 * Scan the built-in rule table and return the highest-severity rule that
 * matches @p event.
 *
 * @param event  Event to evaluate.
 * @return       Pointer to the matching OlRule, or NULL if no rule fires.
 */
const OlRule* ol_rule_check(const OlEvent* event);

/**
 * Return a read-only pointer to the built-in rule table.
 * @param count  Filled with the number of rules.
 */
const OlRule* ol_rule_get_table(size_t* count);

#ifdef __cplusplus
}
#endif
