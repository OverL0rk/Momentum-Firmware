#include "flipper_guard_i.h"

/* ── timer callback ───────────────────────────────────────────────────── */

/**
 * Called from the FuriTimer ISR-safe context every FG_POLL_MS milliseconds.
 * Posts FGuardEventPoll to the view_dispatcher queue; the watch scene's
 * on_event handler processes the actual file check on the FAP task.
 */
static void fg_poll_timer_cb(void* context) {
    FGuardApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, FGuardEventPoll);
}

/* ── view_dispatcher callbacks ────────────────────────────────────────── */

static bool fg_navigation_callback(void* context) {
    FGuardApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static bool fg_custom_event_callback(void* context, uint32_t event) {
    FGuardApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

/* ── alloc / free ─────────────────────────────────────────────────────── */

static FGuardApp* fg_app_alloc(void) {
    FGuardApp* app = malloc(sizeof(FGuardApp));

    app->watch_text     = furi_string_alloc();
    app->last_file_size = 0;
    app->notifications  = furi_record_open(RECORD_NOTIFICATION);

    app->scene_manager = scene_manager_alloc(&flipper_guard_scene_handlers, app);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, fg_navigation_callback);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, fg_custom_event_callback);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, FGuardViewSubmenu, submenu_get_view(app->submenu));

    app->text_box = text_box_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, FGuardViewTextBox, text_box_get_view(app->text_box));

    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, FGuardViewWidget, widget_get_view(app->widget));

    /*
     * Create a periodic timer. It is started/stopped by the watch scene's
     * on_enter / on_exit; here we only allocate it so the scene can use it.
     */
    app->poll_timer =
        furi_timer_alloc(fg_poll_timer_cb, FuriTimerTypePeriodic, app);

    Gui* gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);

    return app;
}

static void fg_app_free(FGuardApp* app) {
    /* Ensure the timer is stopped before freeing */
    furi_timer_stop(app->poll_timer);
    furi_timer_free(app->poll_timer);

    view_dispatcher_remove_view(app->view_dispatcher, FGuardViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, FGuardViewTextBox);
    view_dispatcher_remove_view(app->view_dispatcher, FGuardViewWidget);

    submenu_free(app->submenu);
    text_box_free(app->text_box);
    widget_free(app->widget);

    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    furi_string_free(app->watch_text);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    free(app);
}

/* ── entry point ──────────────────────────────────────────────────────── */

int32_t flipper_guard_app(void* p) {
    UNUSED(p);
    FGuardApp* app = fg_app_alloc();
    scene_manager_next_scene(app->scene_manager, FGuardSceneStart);
    view_dispatcher_run(app->view_dispatcher);
    fg_app_free(app);
    return 0;
}
