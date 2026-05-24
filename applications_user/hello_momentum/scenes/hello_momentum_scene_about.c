#include "../hello_momentum_i.h"

void hello_momentum_scene_about_on_enter(void* context) {
    HelloMomentumApp* app = context;
    Widget* widget = app->widget;

    widget_reset(widget);
    widget_add_string_element(widget, 64, 4, AlignCenter, AlignTop, FontPrimary, "Hello Momentum");
    widget_add_string_element(widget, 64, 18, AlignCenter, AlignTop, FontSecondary, "v0.1");
    widget_add_string_multiline_element(
        widget,
        64,
        32,
        AlignCenter,
        AlignTop,
        FontSecondary,
        "Esqueleto FAP con\nSceneManager + Storage");
    widget_add_string_element(
        widget, 64, 56, AlignCenter, AlignBottom, FontSecondary, "Atras para salir");

    view_dispatcher_switch_to_view(app->view_dispatcher, HelloMomentumViewWidget);
}

bool hello_momentum_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void hello_momentum_scene_about_on_exit(void* context) {
    HelloMomentumApp* app = context;
    widget_reset(app->widget);
}
