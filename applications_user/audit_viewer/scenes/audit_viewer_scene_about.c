#include "../audit_viewer_i.h"

void audit_viewer_scene_about_on_enter(void* context) {
    AuditViewerApp* app = context;
    Widget* widget = app->widget;

    widget_reset(widget);
    widget_add_string_element(widget, 64, 4, AlignCenter, AlignTop, FontPrimary, "Audit Viewer");
    widget_add_string_element(widget, 64, 18, AlignCenter, AlignTop, FontSecondary, "v0.1");
    widget_add_string_multiline_element(
        widget,
        64,
        32,
        AlignCenter,
        AlignTop,
        FontSecondary,
        "Logs at:\n/ext/audit/*.csv");
    widget_add_string_element(
        widget, 64, 56, AlignCenter, AlignBottom, FontSecondary, "Back to exit");

    view_dispatcher_switch_to_view(app->view_dispatcher, AuditViewerViewWidget);
}

bool audit_viewer_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void audit_viewer_scene_about_on_exit(void* context) {
    AuditViewerApp* app = context;
    widget_reset(app->widget);
}
