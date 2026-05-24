#include "../hello_momentum_i.h"

#define GREET_AUTOCLOSE_MS 2000

static void hello_momentum_scene_greet_popup_callback(void* context) {
    HelloMomentumApp* app = context;
    scene_manager_search_and_switch_to_previous_scene(
        app->scene_manager, HelloMomentumSceneStart);
}

void hello_momentum_scene_greet_on_enter(void* context) {
    HelloMomentumApp* app = context;
    Popup* popup = app->popup;

    popup_reset(popup);
    popup_set_header(popup, "Hola!", 64, 18, AlignCenter, AlignTop);
    popup_set_text(popup, "Bienvenido a tu primera\nFAP en Momentum", 64, 36, AlignCenter, AlignTop);
    popup_set_timeout(popup, GREET_AUTOCLOSE_MS);
    popup_enable_timeout(popup);
    popup_set_context(popup, app);
    popup_set_callback(popup, hello_momentum_scene_greet_popup_callback);

    notification_message(app->notifications, &sequence_success);

    view_dispatcher_switch_to_view(app->view_dispatcher, HelloMomentumViewPopup);
}

bool hello_momentum_scene_greet_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void hello_momentum_scene_greet_on_exit(void* context) {
    HelloMomentumApp* app = context;
    popup_reset(app->popup);
}
