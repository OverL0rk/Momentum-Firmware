#pragma once

#include <gui/scene_manager.h>

typedef enum {
#define ADD_SCENE(prefix, name, id) PatternAnalyzerScene##id,
#include "pattern_analyzer_scene_config.h"
#undef ADD_SCENE
    PatternAnalyzerSceneNum,
} PatternAnalyzerScene;

extern const SceneManagerHandlers pattern_analyzer_scene_handlers;

#define ADD_SCENE(prefix, name, id)                                          \
    void prefix##_scene_##name##_on_enter(void* context);                    \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent); \
    void prefix##_scene_##name##_on_exit(void* context);
#include "pattern_analyzer_scene_config.h"
#undef ADD_SCENE
