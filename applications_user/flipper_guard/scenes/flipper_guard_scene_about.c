#include "../flipper_guard_i.h"

void flipper_guard_scene_about_on_enter(void* context) {
    FGuardApp* app    = context;
    Widget*    widget = app->widget;

    widget_reset(widget);
    widget_add_string_element(
        widget, 64, 4, AlignCenter, AlignTop, FontPrimary, "FlipperGuard");
    widget_add_string_element(
        widget, 64, 16, AlignCenter, AlignTop, FontSecondary, "v0.2  by OverL0rk");
    widget_add_string_multiline_element(
        widget,
        64,
        28,
        AlignCenter,
        AlignTop,
        FontSecondary,
        "Live monitor, rule engine.\n"
        "Cyan=Info Yellow=Warn\n"
        "Red=Alert Mgnt=Critical");
    widget_add_string_element(
        widget, 64, 60, AlignCenter, AlignBottom, FontSecondary, "Back to exit");

    view_dispatcher_switch_to_view(app->view_dispatcher, FGuardViewWidget);
}

bool flipper_guard_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void flipper_guard_scene_about_on_exit(void* context) {
    FGuardApp* app = context;
    widget_reset(app->widget);
}
