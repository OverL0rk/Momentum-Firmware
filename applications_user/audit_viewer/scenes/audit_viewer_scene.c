#include "audit_viewer_scene.h"

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
static void (*const audit_viewer_on_enter_handlers[])(void*) = {
#include "audit_viewer_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
static bool (*const audit_viewer_on_event_handlers[])(void*, SceneManagerEvent) = {
#include "audit_viewer_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
static void (*const audit_viewer_on_exit_handlers[])(void*) = {
#include "audit_viewer_scene_config.h"
};
#undef ADD_SCENE

const SceneManagerHandlers audit_viewer_scene_handlers = {
    .on_enter_handlers = audit_viewer_on_enter_handlers,
    .on_event_handlers = audit_viewer_on_event_handlers,
    .on_exit_handlers = audit_viewer_on_exit_handlers,
    .scene_num = AuditViewerSceneNum,
};
