#include "pattern_analyzer_i.h"

#include <furi_hal_rtc.h>
#include <storage/storage.h>
#include <datetime/datetime.h>

static bool pa_nav_cb(void* ctx) {
    PatternAnalyzerApp* app = ctx;
    return scene_manager_handle_back_event(app->scene_manager);
}

static bool pa_custom_cb(void* ctx, uint32_t event) {
    PatternAnalyzerApp* app = ctx;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static PatternAnalyzerApp* pattern_analyzer_app_alloc(void) {
    PatternAnalyzerApp* app = malloc(sizeof(PatternAnalyzerApp));

    app->report = furi_string_alloc();
    app->events = malloc(sizeof(AnalyzerEvent) * PATTERN_ANALYZER_MAX_EVENTS);
    app->event_count = 0;

    app->scene_manager = scene_manager_alloc(&pattern_analyzer_scene_handlers, app);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, pa_nav_cb);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, pa_custom_cb);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, PatternAnalyzerViewSubmenu, submenu_get_view(app->submenu));

    app->text_box = text_box_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, PatternAnalyzerViewTextBox, text_box_get_view(app->text_box));

    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, PatternAnalyzerViewWidget, widget_get_view(app->widget));

    Gui* gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);

    return app;
}

static void pattern_analyzer_app_free(PatternAnalyzerApp* app) {
    view_dispatcher_remove_view(app->view_dispatcher, PatternAnalyzerViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, PatternAnalyzerViewTextBox);
    view_dispatcher_remove_view(app->view_dispatcher, PatternAnalyzerViewWidget);

    submenu_free(app->submenu);
    text_box_free(app->text_box);
    widget_free(app->widget);

    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    free(app->events);
    furi_string_free(app->report);

    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t pattern_analyzer_app(void* p) {
    UNUSED(p);
    PatternAnalyzerApp* app = pattern_analyzer_app_alloc();
    scene_manager_next_scene(app->scene_manager, PatternAnalyzerSceneStart);
    view_dispatcher_run(app->view_dispatcher);
    pattern_analyzer_app_free(app);
    return 0;
}
