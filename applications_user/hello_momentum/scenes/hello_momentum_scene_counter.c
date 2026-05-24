#include "../hello_momentum_i.h"

#define COUNTER_MIN -100
#define COUNTER_MAX  100

static void hello_momentum_scene_counter_update_label(VariableItem* item, int32_t value) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%ld", (long)value);
    variable_item_set_current_value_text(item, buf);
}

static void hello_momentum_scene_counter_value_changed(VariableItem* item) {
    HelloMomentumApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->settings.counter = COUNTER_MIN + (int32_t)idx;
    hello_momentum_scene_counter_update_label(item, app->settings.counter);
}

void hello_momentum_scene_counter_on_enter(void* context) {
    HelloMomentumApp* app = context;
    VariableItemList* list = app->var_item_list;

    variable_item_list_reset(list);

    const uint8_t range = (uint8_t)(COUNTER_MAX - COUNTER_MIN + 1);
    VariableItem* item = variable_item_list_add(
        list, "Valor", range, hello_momentum_scene_counter_value_changed, app);

    int32_t initial = app->settings.counter;
    if(initial < COUNTER_MIN) initial = COUNTER_MIN;
    if(initial > COUNTER_MAX) initial = COUNTER_MAX;
    variable_item_set_current_value_index(item, (uint8_t)(initial - COUNTER_MIN));
    hello_momentum_scene_counter_update_label(item, initial);

    view_dispatcher_switch_to_view(app->view_dispatcher, HelloMomentumViewVarItemList);
}

bool hello_momentum_scene_counter_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void hello_momentum_scene_counter_on_exit(void* context) {
    HelloMomentumApp* app = context;
    hello_momentum_settings_save(&app->settings);
    variable_item_list_reset(app->var_item_list);
}
