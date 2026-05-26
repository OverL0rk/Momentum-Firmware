#pragma once

#include <gui/scene_manager.h>

typedef enum {
#define ADD_SCENE(prefix, name, id) RfScannerScene##id,
#include "rf_scanner_scene_config.h"
#undef ADD_SCENE
    RfScannerSceneNum,
} RfScannerScene;

extern const SceneManagerHandlers rf_scanner_scene_handlers;

#define ADD_SCENE(prefix, name, id)                                                \
    void prefix##_scene_##name##_on_enter(void* context);                          \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event); \
    void prefix##_scene_##name##_on_exit(void* context);
#include "rf_scanner_scene_config.h"
#undef ADD_SCENE
