#pragma once

#include <storage/storage.h>

#define HELLO_MOMENTUM_TAG          "HelloMomentum"
#define HELLO_MOMENTUM_SETTINGS_PATH EXT_PATH("apps_data/hello_momentum/settings.txt")
#define HELLO_MOMENTUM_SETTINGS_DIR  EXT_PATH("apps_data/hello_momentum")

typedef struct {
    int32_t counter;
} HelloMomentumSettings;
