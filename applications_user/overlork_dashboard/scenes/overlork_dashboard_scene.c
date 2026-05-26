#include "overlork_dashboard_scene.h"

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
static void (*const overlork_dashboard_on_enter_handlers[])(void*) = {
#include "overlork_dashboard_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
static bool (*const overlork_dashboard_on_event_handlers[])(void*, SceneManagerEvent) = {
#include "overlork_dashboard_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
static void (*const overlork_dashboard_on_exit_handlers[])(void*) = {
#include "overlork_dashboard_scene_config.h"
};
#undef ADD_SCENE

const SceneManagerHandlers overlork_dashboard_scene_handlers = {
    .on_enter_handlers = overlork_dashboard_on_enter_handlers,
    .on_event_handlers = overlork_dashboard_on_event_handlers,
    .on_exit_handlers = overlork_dashboard_on_exit_handlers,
    .scene_num = OdSceneNum,
};
