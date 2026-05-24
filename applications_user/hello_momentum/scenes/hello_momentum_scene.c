#include "hello_momentum_scene.h"

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
static void (*const hello_momentum_on_enter_handlers[])(void*) = {
#include "hello_momentum_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
static bool (*const hello_momentum_on_event_handlers[])(void*, SceneManagerEvent) = {
#include "hello_momentum_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
static void (*const hello_momentum_on_exit_handlers[])(void*) = {
#include "hello_momentum_scene_config.h"
};
#undef ADD_SCENE

const SceneManagerHandlers hello_momentum_scene_handlers = {
    .on_enter_handlers = hello_momentum_on_enter_handlers,
    .on_event_handlers = hello_momentum_on_event_handlers,
    .on_exit_handlers = hello_momentum_on_exit_handlers,
    .scene_num = HelloMomentumSceneNum,
};
