#pragma once

#include "scenes/flipper_guard_scene.h"

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_box.h>
#include <gui/modules/widget.h>
#include <notification/notification_app.h>
#include <notification/notification_messages.h>
#include <overlork_events/overlork_events.h>
#include <overlork_rules/overlork_rules.h>
#include <overlork_notify/overlork_notify.h>

/** Custom events posted by the periodic poll timer */
typedef enum {
    FGuardEventPoll = 0, /**< File size check tick */
} FGuardEvent;

/** View IDs registered with ViewDispatcher */
typedef enum {
    FGuardViewSubmenu,
    FGuardViewTextBox,
    FGuardViewWidget,
} FGuardView;

/** Central application context */
typedef struct {
    SceneManager*    scene_manager;
    ViewDispatcher*  view_dispatcher;

    Submenu*  submenu;
    TextBox*  text_box;
    Widget*   widget;

    FuriTimer*       poll_timer;          /**< Periodic file-size poll (1 s) */
    uint64_t         last_file_size;      /**< Byte count at last display refresh */
    uint64_t         last_parsed_offset;  /**< Byte offset after last rule-parsed line */
    FuriString*      watch_text;          /**< CSV tail content (without status footer) */
    NotificationApp* notifications;

    /* v0.3 — live severity header fields */
    uint32_t         session_events;    /**< New events counted since entering Watch */
    OlNotifySeverity session_max_sev;   /**< Highest severity seen this session */
} FGuardApp;
