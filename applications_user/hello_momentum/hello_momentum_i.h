#pragma once

#include "hello_momentum.h"
#include "scenes/hello_momentum_scene.h"

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/popup.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

typedef enum {
    HelloMomentumViewSubmenu,
    HelloMomentumViewPopup,
    HelloMomentumViewVarItemList,
    HelloMomentumViewWidget,
} HelloMomentumView;

typedef struct {
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    NotificationApp* notifications;

    Submenu* submenu;
    Popup* popup;
    VariableItemList* var_item_list;
    Widget* widget;

    HelloMomentumSettings settings;
} HelloMomentumApp;

bool hello_momentum_settings_load(HelloMomentumSettings* out);
bool hello_momentum_settings_save(const HelloMomentumSettings* in);
