/**
 * @file pattern_analyzer_scene_export.c
 * @brief Runs analysis then saves the report to the SD card.
 *
 * Saves to: /ext/audit/report-YYYY-MM-DD.txt
 * Overwrites any existing report for today.
 */

#include "../pattern_analyzer_i.h"

void pattern_analyzer_scene_export_on_enter(void* context) {
    PatternAnalyzerApp* app = context;
    Widget* widget = app->widget;

    /* Generate a fresh report (re-reads the log file) */
    pattern_analyzer_run_analysis(app);

    /* Write to SD */
    bool ok = pattern_analyzer_export_report(app);

    widget_reset(widget);
    widget_add_string_element(
        widget, 64, 4, AlignCenter, AlignTop, FontPrimary, "Export Report");

    if(ok) {
        widget_add_string_multiline_element(
            widget,
            64,
            20,
            AlignCenter,
            AlignTop,
            FontSecondary,
            "Report saved!\n\n/ext/audit/\nreport-YYYY-MM-DD.txt");
    } else {
        widget_add_string_multiline_element(
            widget,
            64,
            20,
            AlignCenter,
            AlignTop,
            FontSecondary,
            "Export failed!\n\nCheck SD card\nis inserted.");
    }

    widget_add_string_element(
        widget, 64, 60, AlignCenter, AlignBottom, FontSecondary, "Back to return");

    view_dispatcher_switch_to_view(app->view_dispatcher, PatternAnalyzerViewWidget);
}

bool pattern_analyzer_scene_export_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void pattern_analyzer_scene_export_on_exit(void* context) {
    PatternAnalyzerApp* app = context;
    widget_reset(app->widget);
}
