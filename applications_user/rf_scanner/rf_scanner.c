/**
 * @file rf_scanner.c
 * @brief RF Scanner FAP — entry point, alloc/free, event callbacks.
 *
 * The app uses a standard SceneManager + ViewDispatcher architecture.
 * RF scanning work is performed by a dedicated FuriThread (rf_scanner_worker.c)
 * that posts RfScannerEventScanDone to the ViewDispatcher when finished.
 */

#include "rf_scanner_i.h"
#include <string.h>

/* ── Watch-mode timer callback ────────────────────────────────────────── */

/**
 * Called by the FuriTimer when the watch-mode interval elapses.
 * Runs in the timer ISR context — only safe to post an event.
 */
static void rf_scanner_watch_timer_cb(void* context) {
    RfScannerApp* app = context;
    view_dispatcher_send_custom_event(
        app->view_dispatcher, (uint32_t)RfScannerEventWatchTick);
}

/* ── ViewDispatcher callbacks ─────────────────────────────────────────── */

static bool rf_scanner_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    RfScannerApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool rf_scanner_back_event_callback(void* context) {
    furi_assert(context);
    RfScannerApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

/* ── App lifecycle ────────────────────────────────────────────────────── */

static RfScannerApp* rf_scanner_alloc(void) {
    RfScannerApp* app = malloc(sizeof(RfScannerApp));
    memset(app, 0, sizeof(*app));

    /* Scene manager */
    app->scene_manager = scene_manager_alloc(&rf_scanner_scene_handlers, app);

    /* View dispatcher */
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, rf_scanner_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, rf_scanner_back_event_callback);

    /* Views */
    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, RfScannerViewSubmenu, submenu_get_view(app->submenu));

    app->text_box = text_box_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, RfScannerViewTextBox, text_box_get_view(app->text_box));

    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, RfScannerViewWidget, widget_get_view(app->widget));

    /* Notifications */
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    /* Shared display buffer */
    app->display_text = furi_string_alloc();

    /* Worker thread (not yet started) */
    app->worker = furi_thread_alloc_ex(
        "RfScanWorker", 1024u, rf_scanner_worker_thread, app);

    /* Watch-mode one-shot timer (started/stopped by the scan scene) */
    app->watch_timer = furi_timer_alloc(rf_scanner_watch_timer_cb, FuriTimerTypeOnce, app);

    return app;
}

static void rf_scanner_free(RfScannerApp* app) {
    furi_assert(app);

    /* Ensure the watch timer is stopped before freeing */
    furi_timer_stop(app->watch_timer);
    furi_timer_free(app->watch_timer);

    /* Ensure the worker has stopped before freeing resources */
    rf_scanner_worker_stop(app);
    furi_thread_free(app->worker);

    furi_string_free(app->display_text);
    furi_record_close(RECORD_NOTIFICATION);

    view_dispatcher_remove_view(app->view_dispatcher, RfScannerViewWidget);
    widget_free(app->widget);

    view_dispatcher_remove_view(app->view_dispatcher, RfScannerViewTextBox);
    text_box_free(app->text_box);

    view_dispatcher_remove_view(app->view_dispatcher, RfScannerViewSubmenu);
    submenu_free(app->submenu);

    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    free(app);
}

/* ── Entry point ──────────────────────────────────────────────────────── */

int32_t rf_scanner_app(void* p) {
    UNUSED(p);

    RfScannerApp* app = rf_scanner_alloc();

    Gui* gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    scene_manager_next_scene(app->scene_manager, RfScannerSceneStart);
    view_dispatcher_run(app->view_dispatcher);

    furi_record_close(RECORD_GUI);
    rf_scanner_free(app);
    return 0;
}
