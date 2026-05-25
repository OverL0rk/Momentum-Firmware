#include "audit_viewer_i.h"

static bool audit_viewer_navigation_callback(void* context) {
    AuditViewerApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static bool audit_viewer_custom_event_callback(void* context, uint32_t event) {
    AuditViewerApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static AuditViewerApp* audit_viewer_app_alloc(void) {
    AuditViewerApp* app = malloc(sizeof(AuditViewerApp));

    app->log_text = furi_string_alloc();

    app->scene_manager = scene_manager_alloc(&audit_viewer_scene_handlers, app);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, audit_viewer_navigation_callback);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, audit_viewer_custom_event_callback);

    app->var_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        AuditViewerViewVarItemList,
        variable_item_list_get_view(app->var_item_list));

    app->text_box = text_box_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, AuditViewerViewTextBox, text_box_get_view(app->text_box));

    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, AuditViewerViewWidget, widget_get_view(app->widget));

    app->popup = popup_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, AuditViewerViewPopup, popup_get_view(app->popup));

    Gui* gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);

    return app;
}

static void audit_viewer_app_free(AuditViewerApp* app) {
    view_dispatcher_remove_view(app->view_dispatcher, AuditViewerViewVarItemList);
    view_dispatcher_remove_view(app->view_dispatcher, AuditViewerViewTextBox);
    view_dispatcher_remove_view(app->view_dispatcher, AuditViewerViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, AuditViewerViewPopup);

    variable_item_list_free(app->var_item_list);
    text_box_free(app->text_box);
    widget_free(app->widget);
    popup_free(app->popup);

    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    furi_string_free(app->log_text);

    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t audit_viewer_app(void* p) {
    UNUSED(p);
    AuditViewerApp* app = audit_viewer_app_alloc();
    scene_manager_next_scene(app->scene_manager, AuditViewerSceneStart);
    view_dispatcher_run(app->view_dispatcher);
    audit_viewer_app_free(app);
    return 0;
}
