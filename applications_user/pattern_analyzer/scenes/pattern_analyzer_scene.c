#include "pattern_analyzer_scene.h"

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
static void (*const pattern_analyzer_on_enter_handlers[])(void*) = {
#include "pattern_analyzer_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
static bool (*const pattern_analyzer_on_event_handlers[])(void*, SceneManagerEvent) = {
#include "pattern_analyzer_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
static void (*const pattern_analyzer_on_exit_handlers[])(void*) = {
#include "pattern_analyzer_scene_config.h"
};
#undef ADD_SCENE

const SceneManagerHandlers pattern_analyzer_scene_handlers = {
    .on_enter_handlers = pattern_analyzer_on_enter_handlers,
    .on_event_handlers = pattern_analyzer_on_event_handlers,
    .on_exit_handlers = pattern_analyzer_on_exit_handlers,
    .scene_num = PatternAnalyzerSceneNum,
};
