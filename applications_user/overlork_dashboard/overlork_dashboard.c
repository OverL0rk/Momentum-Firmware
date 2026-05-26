/**
 * @file overlork_dashboard.c
 * @brief OverL0rk Dashboard v0.2 — entry point, alloc/free.
 *
 * v0.2 adds a persistent status-bar indicator on the right side of the
 * status bar (GuiLayerStatusBarRight) that shows today's total event count.
 * The indicator is live while the FAP is running; the count updates on every
 * Stats scene entry.
 */

#include "overlork_dashboard_i.h"
#include <string.h>

/* ── Status-bar indicator ─────────────────────────────────────────────── */

/** Width of the status-bar indicator viewport in pixels. */
#define OD_INDICATOR_WIDTH 14u

/**
 * Draw callback for the status-bar ViewPort.
 * Renders indicator_text (e.g. "42" or "99+") in FontSecondary.
 */
static void od_indicator_draw_cb(Canvas* canvas, void* context) {
    OdApp* app = context;
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 1, 7, app->indicator_text);
}

void od_indicator_update(OdApp* app) {
    uint32_t n = app->stats.total;
    if(n > 99u) {
        snprintf(app->indicator_text, sizeof(app->indicator_text), "99+");
    } else {
        snprintf(app->indicator_text, sizeof(app->indicator_text), "%lu", (unsigned long)n);
    }
    view_port_update(app->indicator_vp);
}

/* ── Event callbacks ──────────────────────────────────────────────────── */

static bool od_navigation_callback(void* context) {
    OdApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static bool od_custom_event_callback(void* context, uint32_t event) {
    OdApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

/* ── App lifecycle ────────────────────────────────────────────────────── */

static OdApp* od_app_alloc(void) {
    OdApp* app = malloc(sizeof(OdApp));
    memset(app, 0, sizeof(*app));

    app->log_text = furi_string_alloc();

    app->scene_manager = scene_manager_alloc(&overlork_dashboard_scene_handlers, app);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, od_navigation_callback);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, od_custom_event_callback);

    /* Views */
    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, OdViewSubmenu, submenu_get_view(app->submenu));

    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, OdViewWidget, widget_get_view(app->widget));

    app->text_box = text_box_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, OdViewTextBox, text_box_get_view(app->text_box));

    /* TOTP custom view — model + draw callback wired by the totp scene. */
    app->totp_view = view_alloc();
    view_dispatcher_add_view(app->view_dispatcher, OdViewTotp, app->totp_view);

    /* GUI — keep the pointer so we can remove the viewport in free() */
    app->gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    /* Status-bar indicator (right side, always-on while FAP runs) */
    snprintf(app->indicator_text, sizeof(app->indicator_text), "0");
    app->indicator_vp = view_port_alloc();
    view_port_set_width(app->indicator_vp, OD_INDICATOR_WIDTH);
    view_port_draw_callback_set(app->indicator_vp, od_indicator_draw_cb, app);
    gui_add_view_port(app->gui, app->indicator_vp, GuiLayerStatusBarRight);

    return app;
}

static void od_app_free(OdApp* app) {
    furi_assert(app);

    /* Remove the status-bar indicator before closing the GUI record */
    gui_remove_view_port(app->gui, app->indicator_vp);
    view_port_free(app->indicator_vp);

    view_dispatcher_remove_view(app->view_dispatcher, OdViewTotp);
    view_dispatcher_remove_view(app->view_dispatcher, OdViewTextBox);
    view_dispatcher_remove_view(app->view_dispatcher, OdViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, OdViewSubmenu);

    view_free(app->totp_view);
    text_box_free(app->text_box);
    widget_free(app->widget);
    submenu_free(app->submenu);

    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    furi_string_free(app->log_text);

    furi_record_close(RECORD_GUI);
    free(app);
}

/* ── Entry point ──────────────────────────────────────────────────────── */

int32_t overlork_dashboard_app(void* p) {
    UNUSED(p);
    OdApp* app = od_app_alloc();
    scene_manager_next_scene(app->scene_manager, OdSceneStart);
    view_dispatcher_run(app->view_dispatcher);
    od_app_free(app);
    return 0;
}
