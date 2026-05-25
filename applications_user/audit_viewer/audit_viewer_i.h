#pragma once

#include "scenes/audit_viewer_scene.h"

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/text_box.h>
#include <gui/modules/widget.h>
#include <gui/modules/popup.h>

typedef enum {
    AuditViewerViewVarItemList,
    AuditViewerViewTextBox,
    AuditViewerViewWidget,
    AuditViewerViewPopup,
} AuditViewerView;

typedef struct {
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;

    VariableItemList* var_item_list;
    TextBox* text_box;
    Widget* widget;
    Popup* popup;

    FuriString* log_text;
} AuditViewerApp;
