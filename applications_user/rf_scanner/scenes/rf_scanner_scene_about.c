#include "../rf_scanner_i.h"

void rf_scanner_scene_about_on_enter(void* context) {
    RfScannerApp* app    = context;
    Widget*       widget = app->widget;

    widget_reset(widget);
    widget_add_string_element(
        widget, 64, 4, AlignCenter, AlignTop, FontPrimary, "RF Scanner");
    widget_add_string_element(
        widget, 64, 16, AlignCenter, AlignTop, FontSecondary, "v0.1  by OverL0rk");
    widget_add_string_multiline_element(
        widget,
        64,
        28,
        AlignCenter,
        AlignTop,
        FontSecondary,
        "Passive RSSI sweep:\n"
        "315 / 433 / 868 / 915\n"
        "Logs signals > -90dBm");
    widget_add_string_element(
        widget, 64, 60, AlignCenter, AlignBottom, FontSecondary, "Back to exit");

    view_dispatcher_switch_to_view(app->view_dispatcher, RfScannerViewWidget);
}

bool rf_scanner_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void rf_scanner_scene_about_on_exit(void* context) {
    RfScannerApp* app = context;
    widget_reset(app->widget);
}
