#pragma once

#include <gui/scene_manager.h>

typedef enum {
#define ADD_SCENE(prefix, name, id) FGuardScene##id,
#include "flipper_guard_scene_config.h"
#undef ADD_SCENE
    FGuardSceneNum,
} FGuardScene;

extern const SceneManagerHandlers flipper_guard_scene_handlers;

#define ADD_SCENE(prefix, name, id)                                                \
    void prefix##_scene_##name##_on_enter(void* context);                          \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event); \
    void prefix##_scene_##name##_on_exit(void* context);
#include "flipper_guard_scene_config.h"
#undef ADD_SCENE
