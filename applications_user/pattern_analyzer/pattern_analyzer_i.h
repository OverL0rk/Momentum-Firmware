#pragma once

#include "scenes/pattern_analyzer_scene.h"

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_box.h>
#include <gui/modules/widget.h>

#define PATTERN_ANALYZER_AUDIT_DIR "/ext/audit"
#define PATTERN_ANALYZER_MAX_EVENTS 200
#define PATTERN_ANALYZER_UID_LEN    33
#define PATTERN_ANALYZER_FIELD_LEN  16

typedef enum {
    PatternAnalyzerViewSubmenu,
    PatternAnalyzerViewTextBox,
    PatternAnalyzerViewWidget,
} PatternAnalyzerView;

typedef struct {
    char subsystem[PATTERN_ANALYZER_FIELD_LEN];
    char operation[PATTERN_ANALYZER_FIELD_LEN];
    char identifier[PATTERN_ANALYZER_UID_LEN];
    uint32_t epoch_seconds;
} AnalyzerEvent;

typedef struct {
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;

    Submenu* submenu;
    TextBox* text_box;
    Widget* widget;

    FuriString* report;
    AnalyzerEvent* events;
    size_t event_count;
} PatternAnalyzerApp;

void pattern_analyzer_run_analysis(PatternAnalyzerApp* app);

/**
 * Export the current report string to /ext/audit/report-YYYY-MM-DD.txt.
 * Runs are idempotent — overwrites any existing file for today.
 * @return true if the file was written successfully.
 */
bool pattern_analyzer_export_report(PatternAnalyzerApp* app);
