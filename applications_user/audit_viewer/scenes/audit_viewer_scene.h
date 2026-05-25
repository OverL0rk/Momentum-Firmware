#pragma once

#include <gui/scene_manager.h>

typedef enum {
#define ADD_SCENE(prefix, name, id) AuditViewerScene##id,
#include "audit_viewer_scene_config.h"
#undef ADD_SCENE
    AuditViewerSceneNum,
} AuditViewerScene;

extern const SceneManagerHandlers audit_viewer_scene_handlers;

#define ADD_SCENE(prefix, name, id)                                          \
    void prefix##_scene_##name##_on_enter(void* context);                    \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent); \
    void prefix##_scene_##name##_on_exit(void* context);
#include "audit_viewer_scene_config.h"
#undef ADD_SCENE
