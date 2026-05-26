#include "rf_scanner_scene.h"

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
static void (*const rf_scanner_on_enter_handlers[])(void*) = {
#include "rf_scanner_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
static bool (*const rf_scanner_on_event_handlers[])(void*, SceneManagerEvent) = {
#include "rf_scanner_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
static void (*const rf_scanner_on_exit_handlers[])(void*) = {
#include "rf_scanner_scene_config.h"
};
#undef ADD_SCENE

const SceneManagerHandlers rf_scanner_scene_handlers = {
    .on_enter_handlers = rf_scanner_on_enter_handlers,
    .on_event_handlers = rf_scanner_on_event_handlers,
    .on_exit_handlers  = rf_scanner_on_exit_handlers,
    .scene_num         = RfScannerSceneNum,
};
