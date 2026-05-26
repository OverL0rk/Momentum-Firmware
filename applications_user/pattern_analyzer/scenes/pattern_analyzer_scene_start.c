#include "../pattern_analyzer_i.h"

typedef enum {
    StartItemRun = 0,
    StartItemExport,
    StartItemAbout,
} StartItem;

static void pa_start_submenu_cb(void* context, uint32_t index) {
    PatternAnalyzerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void pattern_analyzer_scene_start_on_enter(void* context) {
    PatternAnalyzerApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, "Pattern Analyzer");
    submenu_add_item(submenu, "Run analysis", StartItemRun, pa_start_submenu_cb, app);
    submenu_add_item(submenu, "Export report", StartItemExport, pa_start_submenu_cb, app);
    submenu_add_item(submenu, "About", StartItemAbout, pa_start_submenu_cb, app);
    submenu_set_selected_item(
        submenu,
        scene_manager_get_scene_state(app->scene_manager, PatternAnalyzerSceneStart));

    view_dispatcher_switch_to_view(app->view_dispatcher, PatternAnalyzerViewSubmenu);
}

bool pattern_analyzer_scene_start_on_event(void* context, SceneManagerEvent event) {
    PatternAnalyzerApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(
            app->scene_manager, PatternAnalyzerSceneStart, event.event);
        switch(event.event) {
        case StartItemRun:
            scene_manager_next_scene(app->scene_manager, PatternAnalyzerSceneRun);
            consumed = true;
            break;
        case StartItemExport:
            scene_manager_next_scene(app->scene_manager, PatternAnalyzerSceneExport);
            consumed = true;
            break;
        case StartItemAbout:
            scene_manager_next_scene(app->scene_manager, PatternAnalyzerSceneAbout);
            consumed = true;
            break;
        default:
            break;
        }
    }
    return consumed;
}

void pattern_analyzer_scene_start_on_exit(void* context) {
    PatternAnalyzerApp* app = context;
    submenu_reset(app->submenu);
}
