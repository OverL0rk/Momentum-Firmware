#pragma once

#include <gui/scene_manager.h>

typedef enum {
#define ADD_SCENE(prefix, name, id) HelloMomentumScene##id,
#include "hello_momentum_scene_config.h"
#undef ADD_SCENE
    HelloMomentumSceneNum,
} HelloMomentumScene;

extern const SceneManagerHandlers hello_momentum_scene_handlers;

#define ADD_SCENE(prefix, name, id)                                          \
    void prefix##_scene_##name##_on_enter(void* context);                    \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent); \
    void prefix##_scene_##name##_on_exit(void* context);
#include "hello_momentum_scene_config.h"
#undef ADD_SCENE
