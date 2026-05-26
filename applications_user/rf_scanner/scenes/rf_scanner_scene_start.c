#include "../rf_scanner_i.h"

typedef enum {
    StartItemScan  = 0,
    StartItemWatch = 1,
    StartItemAbout = 2,
} StartItem;

static void rf_scanner_scene_start_submenu_cb(void* context, uint32_t index) {
    RfScannerApp* app = context;
    switch(index) {
    case StartItemScan:
        app->watch_mode = false;
        scene_manager_next_scene(app->scene_manager, RfScannerSceneScan);
        break;
    case StartItemWatch:
        /* Enable continuous watch mode — scan re-runs every 5 s automatically */
        app->watch_mode = true;
        scene_manager_next_scene(app->scene_manager, RfScannerSceneScan);
        break;
    case StartItemAbout:
        scene_manager_next_scene(app->scene_manager, RfScannerSceneAbout);
        break;
    default:
        break;
    }
}

void rf_scanner_scene_start_on_enter(void* context) {
    RfScannerApp* app = context;
    Submenu*      sub = app->submenu;

    submenu_reset(sub);
    submenu_set_header(sub, "RF Scanner v0.1");
    submenu_add_item(sub, "Scan Now",    StartItemScan,  rf_scanner_scene_start_submenu_cb, app);
    submenu_add_item(sub, "Watch (5s)", StartItemWatch, rf_scanner_scene_start_submenu_cb, app);
    submenu_add_item(sub, "About",       StartItemAbout, rf_scanner_scene_start_submenu_cb, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, RfScannerViewSubmenu);
}

bool rf_scanner_scene_start_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void rf_scanner_scene_start_on_exit(void* context) {
    RfScannerApp* app = context;
    submenu_reset(app->submenu);
}
