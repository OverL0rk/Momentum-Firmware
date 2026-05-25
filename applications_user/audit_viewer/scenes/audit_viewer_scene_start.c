#include "../audit_viewer_i.h"
#include <audit/audit.h>

typedef enum {
    StartItemToggle,
    StartItemViewLog,
    StartItemAbout,
} StartItem;

static void audit_viewer_scene_start_toggle_callback(VariableItem* item) {
    AuditViewerApp* app = variable_item_get_context(item);
    UNUSED(app);
    uint8_t idx = variable_item_get_current_value_index(item);
    bool enabled = (idx == 1);
    audit_set_enabled(enabled);
    variable_item_set_current_value_text(item, enabled ? "ON" : "OFF");
}

static void audit_viewer_scene_start_enter_callback(void* context, uint32_t index) {
    AuditViewerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void audit_viewer_scene_start_on_enter(void* context) {
    AuditViewerApp* app = context;
    VariableItemList* list = app->var_item_list;

    variable_item_list_reset(list);

    VariableItem* item = variable_item_list_add(
        list, "Audit", 2, audit_viewer_scene_start_toggle_callback, app);
    bool enabled = audit_is_enabled();
    variable_item_set_current_value_index(item, enabled ? 1 : 0);
    variable_item_set_current_value_text(item, enabled ? "ON" : "OFF");

    variable_item_list_add(list, "View today's log", 0, NULL, NULL);
    variable_item_list_add(list, "About", 0, NULL, NULL);

    variable_item_list_set_enter_callback(list, audit_viewer_scene_start_enter_callback, app);
    variable_item_list_set_selected_item(
        list, scene_manager_get_scene_state(app->scene_manager, AuditViewerSceneStart));

    view_dispatcher_switch_to_view(app->view_dispatcher, AuditViewerViewVarItemList);
}

bool audit_viewer_scene_start_on_event(void* context, SceneManagerEvent event) {
    AuditViewerApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, AuditViewerSceneStart, event.event);
        switch(event.event) {
        case StartItemViewLog:
            scene_manager_next_scene(app->scene_manager, AuditViewerSceneLog);
            consumed = true;
            break;
        case StartItemAbout:
            scene_manager_next_scene(app->scene_manager, AuditViewerSceneAbout);
            consumed = true;
            break;
        default:
            break;
        }
    }
    return consumed;
}

void audit_viewer_scene_start_on_exit(void* context) {
    AuditViewerApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
