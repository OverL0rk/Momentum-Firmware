/**
 * @file sentinel_runner.c
 * @brief OverL0rk Sentinel — defensive BadUSB payload index.
 *
 * On launch, extracts every embedded payload to
 * /ext/badusb/OverL0rk_Sentinel/<id>.txt (idempotent — only writes files
 * that don't exist yet or have changed length).
 *
 * UI:
 *   View 1 — Submenu: lists every payload by label.
 *   View 2 — Widget : shows description + the on-disk path so the user
 *                     can hop to BadUSB and run it.
 *
 * Back from Widget returns to Submenu; Back from Submenu exits.
 *
 * Audit hook integration:
 *   When the user runs a Sentinel payload from BadUSB, the modified
 *   ducky_script.c hook detects the "OverL0rk_Sentinel" path segment
 *   and writes "BadKB,AUDIT,<path>,<details>" — distinct from regular
 *   "BadKB,RUN" payloads.
 *
 * Author : Eudys Ramirez (@OverL0rk)
 */

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <storage/storage.h>
#include <audit/audit.h>
#include <string.h>

#include "sentinel_payloads.h"

#define TAG "Sentinel"

#define SENTINEL_DIR     "/ext/badusb/OverL0rk_Sentinel"
#define SENTINEL_PATH_MAX 96

typedef enum {
    SentinelViewMenu,
    SentinelViewDetail,
} SentinelView;

typedef struct {
    Gui*            gui;
    ViewDispatcher* view_dispatcher;
    Submenu*        submenu;
    Widget*         widget;

    const SentinelPayload* payloads;
    size_t                 payload_count;

    /* selected index + buffers held alive while Widget is shown */
    size_t selected;
    char   detail_label[40];
    char   detail_desc[96];
    char   detail_path[SENTINEL_PATH_MAX];
} SentinelApp;

/* ── disk extraction ───────────────────────────────────────────────── */

/**
 * Ensure SENTINEL_DIR exists and every payload is materialised on disk.
 *
 * Strategy:
 *   - Create the directory (no-op if already there).
 *   - For each payload, build the path and check file size.
 *   - If file is missing OR size differs from the in-memory ducky string,
 *     overwrite it.  This keeps payloads in sync when we bump versions.
 */
static void sentinel_extract_payloads(SentinelApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    /* Best-effort mkdir — ignore "already exists" error */
    storage_common_mkdir(storage, SENTINEL_DIR);

    char path[SENTINEL_PATH_MAX];
    for(size_t i = 0; i < app->payload_count; i++) {
        const SentinelPayload* p = &app->payloads[i];
        snprintf(path, sizeof(path), SENTINEL_DIR "/%s.txt", p->id);

        size_t   want_len = strlen(p->ducky);
        FileInfo info;
        info.size = 0;
        bool need_write = true;

        if(storage_file_exists(storage, path)) {
            if(storage_common_stat(storage, path, &info) == FSE_OK &&
               info.size == want_len) {
                /* Same length → assume content unchanged.  This is a heuristic;
                 * a full hash compare would be more robust but slower. */
                need_write = false;
            }
        }

        if(need_write) {
            File* f = storage_file_alloc(storage);
            if(storage_file_open(f, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
                storage_file_write(f, p->ducky, (uint16_t)want_len);
                storage_file_close(f);
                FURI_LOG_I(TAG, "extracted %s (%u bytes)", p->id, (unsigned)want_len);
            } else {
                FURI_LOG_W(TAG, "failed to write %s", path);
            }
            storage_file_free(f);
        }
    }

    furi_record_close(RECORD_STORAGE);
}

/* ── view: detail widget ────────────────────────────────────────────── */

static void sentinel_show_detail(SentinelApp* app, size_t idx) {
    if(idx >= app->payload_count) return;
    const SentinelPayload* p = &app->payloads[idx];

    app->selected = idx;

    snprintf(app->detail_label, sizeof(app->detail_label), "%s", p->label);
    snprintf(app->detail_desc,  sizeof(app->detail_desc),  "%s", p->description);
    snprintf(
        app->detail_path,
        sizeof(app->detail_path),
        "badusb/OverL0rk_Sentinel/\n%s.txt",
        p->id);

    Widget* w = app->widget;
    widget_reset(w);

    /* Header — payload label, accented */
    widget_add_string_element(
        w, 64, 2, AlignCenter, AlignTop, FontPrimary, app->detail_label);

    /* Description (~3 lines, ~16 chars wide) */
    widget_add_text_box_element(
        w, 2, 14, 124, 22, AlignLeft, AlignTop, app->detail_desc, false);

    /* Path hint at the bottom — wrapped for the 18-char-per-line TextBox font */
    widget_add_text_box_element(
        w, 2, 38, 124, 20, AlignLeft, AlignTop, app->detail_path, false);

    /* Footer */
    widget_add_string_element(
        w, 64, 58, AlignCenter, AlignTop, FontSecondary, "[Back] Run from BadUSB");

    /* Log to audit: user is "previewing" the payload — useful audit trail */
    audit_log_event("BadKB", "PREVIEW", p->id, "Sentinel preview shown");

    view_dispatcher_switch_to_view(app->view_dispatcher, SentinelViewDetail);
}

/* ── view: submenu ──────────────────────────────────────────────────── */

static void sentinel_submenu_cb(void* context, uint32_t index) {
    SentinelApp* app = context;
    sentinel_show_detail(app, (size_t)index);
}

static void sentinel_build_submenu(SentinelApp* app) {
    Submenu* s = app->submenu;
    submenu_reset(s);
    submenu_set_header(s, "OverL0rk Sentinel");

    for(size_t i = 0; i < app->payload_count; i++) {
        submenu_add_item(
            s, app->payloads[i].label, (uint32_t)i, sentinel_submenu_cb, app);
    }
}

/* ── view dispatcher callbacks ─────────────────────────────────────── */

static bool sentinel_back_cb(void* context) {
    SentinelApp* app = context;
    uint32_t     cur = view_dispatcher_get_current_view(app->view_dispatcher);

    if(cur == SentinelViewDetail) {
        /* Back from detail → return to submenu */
        view_dispatcher_switch_to_view(app->view_dispatcher, SentinelViewMenu);
        return true;
    }

    /* Back from menu → exit the app */
    view_dispatcher_stop(app->view_dispatcher);
    return true;
}

static bool sentinel_custom_cb(void* context, uint32_t event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

/* ── alloc / free ──────────────────────────────────────────────────── */

static SentinelApp* sentinel_app_alloc(void) {
    SentinelApp* app = malloc(sizeof(SentinelApp));
    memset(app, 0, sizeof(*app));

    app->payloads = sentinel_payloads_get(&app->payload_count);

    app->gui              = furi_record_open(RECORD_GUI);
    app->view_dispatcher  = view_dispatcher_alloc();

    view_dispatcher_attach_to_gui(
        app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, sentinel_back_cb);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, sentinel_custom_cb);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, SentinelViewMenu, submenu_get_view(app->submenu));

    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, SentinelViewDetail, widget_get_view(app->widget));

    return app;
}

static void sentinel_app_free(SentinelApp* app) {
    view_dispatcher_remove_view(app->view_dispatcher, SentinelViewMenu);
    submenu_free(app->submenu);

    view_dispatcher_remove_view(app->view_dispatcher, SentinelViewDetail);
    widget_free(app->widget);

    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);

    free(app);
}

/* ── entry point ───────────────────────────────────────────────────── */

int32_t sentinel_runner_app(void* p) {
    UNUSED(p);

    SentinelApp* app = sentinel_app_alloc();

    /* Extract / refresh payloads on disk before showing the menu */
    sentinel_extract_payloads(app);

    sentinel_build_submenu(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, SentinelViewMenu);

    view_dispatcher_run(app->view_dispatcher);

    sentinel_app_free(app);
    return 0;
}
