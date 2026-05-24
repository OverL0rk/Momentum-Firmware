#include "hello_momentum_i.h"

#include <flipper_format/flipper_format.h>

static const char* SETTINGS_HEADER = "Hello Momentum Settings";
static const uint32_t SETTINGS_VERSION = 1;

bool hello_momentum_settings_load(HelloMomentumSettings* out) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(storage);
    bool ok = false;
    FuriString* header = furi_string_alloc();

    do {
        if(!flipper_format_file_open_existing(ff, HELLO_MOMENTUM_SETTINGS_PATH)) break;
        uint32_t version = 0;
        if(!flipper_format_read_header(ff, header, &version)) break;
        if(version != SETTINGS_VERSION) break;
        if(!flipper_format_read_int32(ff, "counter", &out->counter, 1)) break;
        ok = true;
    } while(false);

    furi_string_free(header);
    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

bool hello_momentum_settings_save(const HelloMomentumSettings* in) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, HELLO_MOMENTUM_SETTINGS_DIR);

    FlipperFormat* ff = flipper_format_file_alloc(storage);
    bool ok = false;

    do {
        if(!flipper_format_file_open_always(ff, HELLO_MOMENTUM_SETTINGS_PATH)) break;
        if(!flipper_format_write_header_cstr(ff, SETTINGS_HEADER, SETTINGS_VERSION)) break;
        if(!flipper_format_write_int32(ff, "counter", &in->counter, 1)) break;
        ok = true;
    } while(false);

    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

static bool hello_momentum_navigation_callback(void* context) {
    furi_assert(context);
    HelloMomentumApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static bool hello_momentum_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    HelloMomentumApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static void hello_momentum_tick_event_callback(void* context) {
    furi_assert(context);
    HelloMomentumApp* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

static HelloMomentumApp* hello_momentum_app_alloc(void) {
    HelloMomentumApp* app = malloc(sizeof(HelloMomentumApp));

    app->settings.counter = 0;
    hello_momentum_settings_load(&app->settings);

    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->scene_manager = scene_manager_alloc(&hello_momentum_scene_handlers, app);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, hello_momentum_navigation_callback);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, hello_momentum_custom_event_callback);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, hello_momentum_tick_event_callback, 100);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, HelloMomentumViewSubmenu, submenu_get_view(app->submenu));

    app->popup = popup_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, HelloMomentumViewPopup, popup_get_view(app->popup));

    app->var_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        HelloMomentumViewVarItemList,
        variable_item_list_get_view(app->var_item_list));

    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, HelloMomentumViewWidget, widget_get_view(app->widget));

    Gui* gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);

    return app;
}

static void hello_momentum_app_free(HelloMomentumApp* app) {
    furi_assert(app);

    hello_momentum_settings_save(&app->settings);

    view_dispatcher_remove_view(app->view_dispatcher, HelloMomentumViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, HelloMomentumViewPopup);
    view_dispatcher_remove_view(app->view_dispatcher, HelloMomentumViewVarItemList);
    view_dispatcher_remove_view(app->view_dispatcher, HelloMomentumViewWidget);

    submenu_free(app->submenu);
    popup_free(app->popup);
    variable_item_list_free(app->var_item_list);
    widget_free(app->widget);

    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t hello_momentum_app(void* p) {
    UNUSED(p);
    HelloMomentumApp* app = hello_momentum_app_alloc();

    scene_manager_next_scene(app->scene_manager, HelloMomentumSceneStart);
    view_dispatcher_run(app->view_dispatcher);

    hello_momentum_app_free(app);
    return 0;
}
