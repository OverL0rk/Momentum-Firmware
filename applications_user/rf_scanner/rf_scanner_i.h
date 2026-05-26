#pragma once

#include "scenes/rf_scanner_scene.h"

#include <furi.h>
#include <furi_hal_subghz.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_box.h>
#include <gui/modules/widget.h>
#include <notification/notification_app.h>
#include <notification/notification_messages.h>
#include <audit/audit.h>

/** RSSI threshold: signals above this are logged/flagged (dBm) */
#define RF_SCAN_THRESHOLD_DBM (-90.0f)

/** Number of fixed frequencies in the sweep */
#define RF_SCAN_FREQ_COUNT 4u

/** Scan result for a single frequency */
typedef struct {
    uint32_t freq_hz;    /**< Frequency in Hz                        */
    float    rssi;       /**< Measured RSSI in dBm                   */
    bool     detected;   /**< true when rssi > RF_SCAN_THRESHOLD_DBM */
} RfScanResult;

/** Interval between automatic re-scans in watch mode (ms) */
#define RF_SCAN_WATCH_INTERVAL_MS 5000u

/**
 * Number of consecutive sweeps a frequency must be above the RSSI threshold
 * before it is classified as a "persistent" (stationary) source and logged
 * as a SubGhz/PERSIST event.  Inspired by the TPMS/ProtoView community
 * finding that continuous transmitters have distinct persistence signatures.
 */
#define RF_SCAN_PERSIST_MIN 3u

/** Custom events posted to the view dispatcher */
typedef enum {
    RfScannerEventScanDone  = 0, /**< Worker thread finished one sweep         */
    RfScannerEventWatchTick = 1, /**< Watch-mode countdown elapsed; re-trigger */
} RfScannerEvent;

/** View IDs registered with ViewDispatcher */
typedef enum {
    RfScannerViewSubmenu = 0,
    RfScannerViewTextBox,
    RfScannerViewWidget,
} RfScannerView;

/** Central application context */
typedef struct {
    SceneManager*   scene_manager;
    ViewDispatcher* view_dispatcher;

    Submenu* submenu;
    TextBox* text_box;
    Widget*  widget;

    NotificationApp* notifications;

    FuriThread*   worker;
    volatile bool scan_stop;      /**< Set to true to request early exit      */
    volatile bool worker_running; /**< Cleared by worker on completion        */

    /**
     * Watch mode: when true the scan scene automatically re-triggers
     * a new sweep RF_SCAN_WATCH_INTERVAL_MS after results are shown.
     */
    bool       watch_mode;
    FuriTimer* watch_timer; /**< One-shot timer that fires RfScannerEventWatchTick */

    RfScanResult results[RF_SCAN_FREQ_COUNT]; /**< Per-frequency results     */
    uint8_t      detected_count;             /**< Signals above threshold in current sweep */

    /**
     * Cross-sweep persistence tracking (watch mode).
     *
     * signal_history[i]: consecutive sweeps that frequency i was above
     *   RF_SCAN_THRESHOLD_DBM.  Reset to 0 when signal disappears.
     *
     * persistent_mask: bitmask — bit i set when signal_history[i] first
     *   reached RF_SCAN_PERSIST_MIN.  Cleared when the signal drops.
     *   Used to log the PERSIST event exactly once per detection session.
     */
    uint8_t signal_history[RF_SCAN_FREQ_COUNT];
    uint8_t persistent_mask;

    FuriString* display_text; /**< Content shown in the scan TextBox */
} RfScannerApp;

/* Worker API (rf_scanner_worker.c) */
int32_t rf_scanner_worker_thread(void* context);
void    rf_scanner_worker_start(RfScannerApp* app);
void    rf_scanner_worker_stop(RfScannerApp* app);
