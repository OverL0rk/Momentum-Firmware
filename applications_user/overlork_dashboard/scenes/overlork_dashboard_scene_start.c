#include "../overlork_dashboard_i.h"

typedef enum {
    OdStartItemStats = 0,
    OdStartItemLog,
    OdStartItemTotp,
    OdStartItemExport,
    OdStartItemAbout,
} OdStartItem;

static void od_scene_start_submenu_callback(void* context, uint32_t index) {
    OdApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void overlork_dashboard_scene_start_on_enter(void* context) {
    OdApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, "OverL0rk Dashboard");

    submenu_add_item(
        submenu, "Stats", OdStartItemStats, od_scene_start_submenu_callback, app);
    submenu_add_item(
        submenu, "Today's Log", OdStartItemLog, od_scene_start_submenu_callback, app);
    submenu_add_item(
        submenu, "TOTP", OdStartItemTotp, od_scene_start_submenu_callback, app);
    submenu_add_item(
        submenu, "Export JSON", OdStartItemExport, od_scene_start_submenu_callback, app);
    submenu_add_item(
        submenu, "About", OdStartItemAbout, od_scene_start_submenu_callback, app);

    submenu_set_selected_item(
        submenu,
        scene_manager_get_scene_state(app->scene_manager, OdSceneStart));

    view_dispatcher_switch_to_view(app->view_dispatcher, OdViewSubmenu);
}

bool overlork_dashboard_scene_start_on_event(void* context, SceneManagerEvent event) {
    OdApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, OdSceneStart, event.event);
        switch(event.event) {
        case OdStartItemStats:
            scene_manager_next_scene(app->scene_manager, OdSceneStats);
            consumed = true;
            break;
        case OdStartItemLog:
            scene_manager_next_scene(app->scene_manager, OdSceneLog);
            consumed = true;
            break;
        case OdStartItemTotp:
            scene_manager_next_scene(app->scene_manager, OdSceneTotp);
            consumed = true;
            break;
        case OdStartItemExport:
            scene_manager_next_scene(app->scene_manager, OdSceneExport);
            consumed = true;
            break;
        case OdStartItemAbout:
            scene_manager_next_scene(app->scene_manager, OdSceneAbout);
            consumed = true;
            break;
        default:
            break;
        }
    }
    return consumed;
}

void overlork_dashboard_scene_start_on_exit(void* context) {
    OdApp* app = context;
    submenu_reset(app->submenu);
}
