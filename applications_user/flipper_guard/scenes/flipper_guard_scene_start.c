#include "../flipper_guard_i.h"

typedef enum {
    FGuardStartItemWatch = 0,
    FGuardStartItemAbout,
} FGuardStartItem;

static void fg_start_submenu_cb(void* context, uint32_t index) {
    FGuardApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void flipper_guard_scene_start_on_enter(void* context) {
    FGuardApp* app = context;
    Submenu*   submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, "FlipperGuard");
    submenu_add_item(
        submenu, "Start watching", FGuardStartItemWatch, fg_start_submenu_cb, app);
    submenu_add_item(
        submenu, "About", FGuardStartItemAbout, fg_start_submenu_cb, app);
    submenu_set_selected_item(
        submenu,
        scene_manager_get_scene_state(app->scene_manager, FGuardSceneStart));

    view_dispatcher_switch_to_view(app->view_dispatcher, FGuardViewSubmenu);
}

bool flipper_guard_scene_start_on_event(void* context, SceneManagerEvent event) {
    FGuardApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(
            app->scene_manager, FGuardSceneStart, event.event);
        switch(event.event) {
        case FGuardStartItemWatch:
            scene_manager_next_scene(app->scene_manager, FGuardSceneWatch);
            consumed = true;
            break;
        case FGuardStartItemAbout:
            scene_manager_next_scene(app->scene_manager, FGuardSceneAbout);
            consumed = true;
            break;
        default:
            break;
        }
    }
    return consumed;
}

void flipper_guard_scene_start_on_exit(void* context) {
    FGuardApp* app = context;
    submenu_reset(app->submenu);
}
