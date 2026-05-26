#include "../pattern_analyzer_i.h"

void pattern_analyzer_scene_about_on_enter(void* context) {
    PatternAnalyzerApp* app = context;
    Widget* widget = app->widget;

    widget_reset(widget);
    widget_add_string_element(
        widget, 64, 4, AlignCenter, AlignTop, FontPrimary, "Pattern Analyzer");
    widget_add_string_element(widget, 64, 18, AlignCenter, AlignTop, FontSecondary, "v0.2");
    widget_add_string_multiline_element(
        widget,
        64,
        30,
        AlignCenter,
        AlignTop,
        FontSecondary,
        "7 heuristics on\naudit.csv\nSeq.UIDs/Prefix/Burst\nCross-proto/TopIDs");
    widget_add_string_element(
        widget, 64, 56, AlignCenter, AlignBottom, FontSecondary, "Back to return");

    view_dispatcher_switch_to_view(app->view_dispatcher, PatternAnalyzerViewWidget);
}

bool pattern_analyzer_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void pattern_analyzer_scene_about_on_exit(void* context) {
    PatternAnalyzerApp* app = context;
    widget_reset(app->widget);
}
