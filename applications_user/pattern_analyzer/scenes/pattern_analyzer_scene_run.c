#include "../pattern_analyzer_i.h"

void pattern_analyzer_scene_run_on_enter(void* context) {
    PatternAnalyzerApp* app = context;

    pattern_analyzer_run_analysis(app);

    TextBox* tb = app->text_box;
    text_box_reset(tb);
    text_box_set_font(tb, TextBoxFontText);
    text_box_set_focus(tb, TextBoxFocusStart);
    text_box_set_text(tb, furi_string_get_cstr(app->report));

    view_dispatcher_switch_to_view(app->view_dispatcher, PatternAnalyzerViewTextBox);
}

bool pattern_analyzer_scene_run_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void pattern_analyzer_scene_run_on_exit(void* context) {
    PatternAnalyzerApp* app = context;
    text_box_reset(app->text_box);
}
