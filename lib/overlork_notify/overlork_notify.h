/**
 * @file overlork_notify.h
 * @brief Severity-aware notification helper for OverL0rk security events.
 *
 * Maps OlRuleAction severity levels to distinct LED + vibration patterns
 * using the Flipper Zero notification service.
 *
 * Severity → LED colour → vibration:
 *   Info     → Cyan  100 ms → none
 *   Warn     → Yellow 100 ms → none
 *   Alert    → Red 100 ms   → none
 *   Critical → Magenta 100 ms + single vibro pulse
 *
 * Usage:
 *   ol_notify(app->notifications, OlNotifyAlert);
 */

#pragma once

#include <notification/notification_app.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Severity levels for ol_notify().
 *
 * Values deliberately match OlRuleAction so they can be cast interchangeably:
 *   ol_notify(notif, (OlNotifySeverity)rule->action);
 */
typedef enum {
    OlNotifyInfo     = 0, /**< Routine event — cyan LED */
    OlNotifyWarn     = 1, /**< Noteworthy — yellow LED */
    OlNotifyAlert    = 2, /**< Suspicious — red LED */
    OlNotifyCritical = 3, /**< High-risk — magenta LED + vibro */
} OlNotifySeverity;

/**
 * Fire a notification with the LED colour and vibration appropriate for
 * @p severity.
 *
 * @param notifications  NotificationApp record (from furi_record_open).
 * @param severity       Desired severity level.
 */
void ol_notify(NotificationApp* notifications, OlNotifySeverity severity);

#ifdef __cplusplus
}
#endif
