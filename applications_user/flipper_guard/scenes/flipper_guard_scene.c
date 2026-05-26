#include "flipper_guard_scene.h"

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
static void (*const flipper_guard_on_enter_handlers[])(void*) = {
#include "flipper_guard_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
static bool (*const flipper_guard_on_event_handlers[])(void*, SceneManagerEvent) = {
#include "flipper_guard_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
static void (*const flipper_guard_on_exit_handlers[])(void*) = {
#include "flipper_guard_scene_config.h"
};
#undef ADD_SCENE

const SceneManagerHandlers flipper_guard_scene_handlers = {
    .on_enter_handlers = flipper_guard_on_enter_handlers,
    .on_event_handlers = flipper_guard_on_event_handlers,
    .on_exit_handlers  = flipper_guard_on_exit_handlers,
    .scene_num         = FGuardSceneNum,
};
