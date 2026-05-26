/**
 * @file overlork_dashboard_scene_about.c
 * @brief About scene — displays author credit and suite summary.
 *
 * Layout (128 × 64 px):
 *   y= 4   "OL Dashboard v0.2"     FontPrimary  centered
 *   y=16   "by Eudys Ramirez"      FontSecondary centered
 *   y=24   "@OverL0rk"             FontSecondary centered
 *   y=34   "6 libs · 5 FAPs"       FontSecondary centered
 *   y=42   "6 hooks  •  audit CSV" FontSecondary centered
 *   y=60   "Back to exit"          FontSecondary centered
 */

#include "../overlork_dashboard_i.h"

void overlork_dashboard_scene_about_on_enter(void* context) {
    OdApp*  app    = context;
    Widget* widget = app->widget;

    widget_reset(widget);

    /* Title */
    widget_add_string_element(
        widget, 64, 4, AlignCenter, AlignTop, FontPrimary, "OL Dashboard v0.2");

    /* Author credits */
    widget_add_string_element(
        widget, 64, 17, AlignCenter, AlignTop, FontSecondary, "by Eudys Ramirez");
    widget_add_string_element(
        widget, 64, 25, AlignCenter, AlignTop, FontSecondary, "@OverL0rk  2026");

    /* Suite summary */
    widget_add_string_element(
        widget, 64, 35, AlignCenter, AlignTop, FontSecondary, "6 libs \xb7 5 FAPs \xb7 6 hooks");
    widget_add_string_element(
        widget, 64, 43, AlignCenter, AlignTop, FontSecondary, "Momentum Firmware");

    /* Exit hint */
    widget_add_string_element(
        widget, 64, 60, AlignCenter, AlignBottom, FontSecondary, "Back to exit");

    view_dispatcher_switch_to_view(app->view_dispatcher, OdViewWidget);
}

bool overlork_dashboard_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void overlork_dashboard_scene_about_on_exit(void* context) {
    OdApp* app = context;
    widget_reset(app->widget);
}
