#include "overlork_notify.h"

#include <notification/notification_messages.h>

void ol_notify(NotificationApp* notifications, OlNotifySeverity severity) {
    if(!notifications) return;

    switch(severity) {
    case OlNotifyInfo:
        notification_message(notifications, &sequence_blink_cyan_100);
        break;
    case OlNotifyWarn:
        notification_message(notifications, &sequence_blink_yellow_100);
        break;
    case OlNotifyAlert:
        notification_message(notifications, &sequence_blink_red_100);
        break;
    case OlNotifyCritical:
        /* Magenta blink AND vibration for maximum visibility */
        notification_message(notifications, &sequence_blink_magenta_100);
        notification_message(notifications, &sequence_single_vibro);
        break;
    default:
        notification_message(notifications, &sequence_blink_cyan_100);
        break;
    }
}
