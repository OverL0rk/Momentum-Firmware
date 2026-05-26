#pragma once

#include "scenes/overlork_dashboard_scene.h"

#include <furi.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/view_port.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <gui/modules/text_box.h>

/** Parsed statistics from today's audit CSV */
typedef struct {
    uint32_t total;    /**< Total events logged today */
    uint32_t nfc;      /**< NFC READ events              */
    uint32_t subghz;   /**< SubGhz RX events             */
    uint32_t ir;       /**< IR TX events                 */
    uint32_t badkb;    /**< BadKB RUN events             */
    uint32_t lfrfid;   /**< LFRFID READ events           */
    uint32_t ibutton;  /**< iButton READ events          */
    char     last_time[9];    /**< "HH:MM:SS\0" of most recent event */
    char     last_subsys[16]; /**< Subsystem name of most recent event */
    uint8_t  battery_pct;     /**< Battery percentage at time of refresh */
} OdStats;

/** View IDs registered with ViewDispatcher */
typedef enum {
    OdViewSubmenu,
    OdViewWidget,
    OdViewTextBox,
    OdViewTotp,
} OdView;

/** Maximum decoded TOTP key length in bytes (base32 "totp.key" ~ <=128 chars). */
#define OD_TOTP_KEY_MAX 80u

/** Central application context */
typedef struct {
    SceneManager*    scene_manager;
    ViewDispatcher*  view_dispatcher;
    Gui*             gui; /**< Kept open for ViewPort lifecycle management */

    Submenu*  submenu;
    Widget*   widget;
    TextBox*  text_box;
    View*     totp_view;

    /* TOTP scene runtime state — written on scene_enter, read by timer. */
    FuriTimer* totp_timer;
    uint8_t    totp_key[OD_TOTP_KEY_MAX];
    uint8_t    totp_key_len;
    bool       totp_no_key;

    /**
     * Status-bar indicator viewport (GuiLayerStatusBarRight).
     * Renders indicator_text — updated after each od_compute_stats() call.
     */
    ViewPort* indicator_vp;
    char      indicator_text[8]; /**< e.g. "42" or "99+"  */

    OdStats     stats;
    FuriString* log_text;

    /*
     * Pre-formatted display strings for the stats widget.
     * These must outlive the on_enter call because widget_add_string_element()
     * stores a pointer, not a copy.
     */
    char disp_total[32];
    char disp_nfc_subghz[40];
    char disp_ir_badkb[40];
    char disp_last_time[32];
    char disp_last_by[36];
} OdApp;

/** Refresh the status-bar indicator with the latest stats.total value. */
void od_indicator_update(OdApp* app);

/** Parse today's audit CSV and populate stats (called from stats scene) */
void od_compute_stats(OdStats* stats);
