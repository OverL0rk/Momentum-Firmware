#include "../hello_momentum_i.h"

typedef enum {
    StartSubmenuGreet,
    StartSubmenuCounter,
    StartSubmenuAbout,
} StartSubmenuIndex;

static void hello_momentum_scene_start_submenu_callback(void* context, uint32_t index) {
    HelloMomentumApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void hello_momentum_scene_start_on_enter(void* context) {
    HelloMomentumApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, "Hello Momentum");
    submenu_add_item(
        submenu, "Saludar", StartSubmenuGreet, hello_momentum_scene_start_submenu_callback, app);
    submenu_add_item(
        submenu,
        "Contador",
        StartSubmenuCounter,
        hello_momentum_scene_start_submenu_callback,
        app);
    submenu_add_item(
        submenu, "Acerca de", StartSubmenuAbout, hello_momentum_scene_start_submenu_callback, app);

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, HelloMomentumSceneStart));

    view_dispatcher_switch_to_view(app->view_dispatcher, HelloMomentumViewSubmenu);
}

bool hello_momentum_scene_start_on_event(void* context, SceneManagerEvent event) {
    HelloMomentumApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, HelloMomentumSceneStart, event.event);
        switch(event.event) {
        case StartSubmenuGreet:
            scene_manager_next_scene(app->scene_manager, HelloMomentumSceneGreet);
            consumed = true;
            break;
        case StartSubmenuCounter:
            scene_manager_next_scene(app->scene_manager, HelloMomentumSceneCounter);
            consumed = true;
            break;
        case StartSubmenuAbout:
            scene_manager_next_scene(app->scene_manager, HelloMomentumSceneAbout);
            consumed = true;
            break;
        default:
            break;
        }
    }
    return consumed;
}

void hello_momentum_scene_start_on_exit(void* context) {
    HelloMomentumApp* app = context;
    submenu_reset(app->submenu);
}
