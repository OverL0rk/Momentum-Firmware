/**
 * @file overlork_dashboard_scene_stats.c
 * @brief Stats widget scene — computes and renders audit summary (v0.2).
 *
 * Layout (128×64 px, fullscreen):
 *   y= 2  "OverL0rk Dash"          FontPrimary  centered
 *   y=14  "Total today: N"          FontSecondary left
 *   y=22  "NFC:N  Sub:N  IR:N"     FontSecondary left
 *   y=30  "LF:N   iBtn:N  KB:N"    FontSecondary left
 *   y=40  "Last: HH:MM:SS"          FontSecondary left
 *   y=50  "by SUBSYS   Bat:N%"      FontSecondary left
 */

#include "../overlork_dashboard_i.h"
#include <string.h>

void overlork_dashboard_scene_stats_on_enter(void* context) {
    OdApp* app = context;

    /* Fresh stats on every enter so data is current */
    od_compute_stats(&app->stats);

    /* Refresh the status-bar indicator with the new total */
    od_indicator_update(app);

    /* Format display strings into the app struct buffers.
     * widget_add_string_element() holds a pointer, not a copy,
     * so the buffers must outlive this function. */
    snprintf(
        app->disp_total,
        sizeof(app->disp_total),
        "Total today: %lu",
        (unsigned long)app->stats.total);

    /* Row 1: NFC / SubGhz / IR — abbreviated to fit in 128px */
    snprintf(
        app->disp_nfc_subghz,
        sizeof(app->disp_nfc_subghz),
        "NFC:%-2lu Sub:%-2lu IR:%-2lu",
        (unsigned long)app->stats.nfc,
        (unsigned long)app->stats.subghz,
        (unsigned long)app->stats.ir);

    /* Row 2: LFRFID / iButton / BadKB */
    snprintf(
        app->disp_ir_badkb,
        sizeof(app->disp_ir_badkb),
        "LF:%-2lu  iBtn:%-2lu KB:%-2lu",
        (unsigned long)app->stats.lfrfid,
        (unsigned long)app->stats.ibutton,
        (unsigned long)app->stats.badkb);

    snprintf(
        app->disp_last_time,
        sizeof(app->disp_last_time),
        "Last: %s",
        app->stats.last_time[0] ? app->stats.last_time : "--:--:--");

    snprintf(
        app->disp_last_by,
        sizeof(app->disp_last_by),
        "by %s   Bat:%u%%",
        app->stats.last_subsys[0] ? app->stats.last_subsys : "N/A",
        (unsigned)app->stats.battery_pct);

    Widget* widget = app->widget;
    widget_reset(widget);

    widget_add_string_element(
        widget, 64, 2, AlignCenter, AlignTop, FontPrimary, "OverL0rk Dash");
    widget_add_string_element(
        widget, 2, 14, AlignLeft, AlignTop, FontSecondary, app->disp_total);
    widget_add_string_element(
        widget, 2, 22, AlignLeft, AlignTop, FontSecondary, app->disp_nfc_subghz);
    widget_add_string_element(
        widget, 2, 30, AlignLeft, AlignTop, FontSecondary, app->disp_ir_badkb);
    widget_add_string_element(
        widget, 2, 40, AlignLeft, AlignTop, FontSecondary, app->disp_last_time);
    widget_add_string_element(
        widget, 2, 50, AlignLeft, AlignTop, FontSecondary, app->disp_last_by);

    view_dispatcher_switch_to_view(app->view_dispatcher, OdViewWidget);
}

bool overlork_dashboard_scene_stats_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void overlork_dashboard_scene_stats_on_exit(void* context) {
    OdApp* app = context;
    widget_reset(app->widget);
}
