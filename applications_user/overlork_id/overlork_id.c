/**
 * @file overlork_id.c
 * @brief OverL0rk Identity Card — personal brand / digital signature FAP.
 *
 * A full-screen "hacker business card" that renders the author's identity
 * and a summary of the OverL0rk Security Suite built for Momentum Firmware.
 *
 * Screen layout (128 × 64 px, monochrome):
 *
 *  ╔══════════════════════════════╗  ← rounded frame
 *  ║      OverL0rk  Suite        ║  ← FontPrimary, centered  y=5
 *  ╠══════════════════════════════╣  ← accent line            y=13
 *  ║  Author : Eudys Ramirez     ║  ←                        y=22
 *  ║  Handle : @OverL0rk         ║  ←  FontSecondary         y=30
 *  ║  Build  : Momentum  2026    ║  ←                        y=38
 *  ╠══════════════════════════════╣  ← accent line            y=47
 *  ║   6 libs · 5 FAPs · 6 hooks ║  ← stats                  y=53
 *  ╚══════════════════════════════╝
 *
 * Press any key to exit.
 *
 * Author : Eudys Ramirez
 * Handle : @OverL0rk
 * Suite  : OverL0rk Security Suite for Momentum Firmware
 * Version: 1.0
 */

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <input/input.h>

/* ── Draw callback ────────────────────────────────────────────────────── */

static void ol_id_draw_cb(Canvas* canvas, void* context) {
    UNUSED(context);
    canvas_clear(canvas);

    /* Outer rounded frame */
    canvas_draw_rframe(canvas, 0, 0, 128, 64, 3);

    /* Header accent lines (top and bottom of body) */
    canvas_draw_line(canvas, 1, 13, 126, 13);
    canvas_draw_line(canvas, 1, 47, 126, 47);

    /* ── Title ── */
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 5, AlignCenter, AlignTop, "OverL0rk Suite");

    /* ── Identity block ── */
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 5, 22, "Author : Eudys Ramirez");
    canvas_draw_str(canvas, 5, 30, "Handle : @OverL0rk");
    canvas_draw_str(canvas, 5, 38, "Build  : Momentum FW 2026");

    /* ── Suite stats ── */
    canvas_draw_str_aligned(
        canvas, 64, 50, AlignCenter, AlignTop, "6 libs \xb7 5 FAPs \xb7 6 hooks");

    /* ── Exit hint — subtle, bottom-right ── */
    canvas_draw_str_aligned(canvas, 125, 62, AlignRight, AlignBottom, "[ any key ]");
}

/* ── Input callback ───────────────────────────────────────────────────── */

static void ol_id_input_cb(InputEvent* event, void* context) {
    FuriMessageQueue* queue = context;
    /* Only enqueue actual key presses (ignore repeats/releases) */
    if(event->type == InputTypePress) {
        furi_message_queue_put(queue, event, 0);
    }
}

/* ── Entry point ──────────────────────────────────────────────────────── */

int32_t overlork_id_app(void* p) {
    UNUSED(p);

    /* Single-slot queue — we just need "any key pressed" */
    FuriMessageQueue* queue = furi_message_queue_alloc(1, sizeof(InputEvent));

    ViewPort* vp = view_port_alloc();
    view_port_draw_callback_set(vp, ol_id_draw_cb, NULL);
    view_port_input_callback_set(vp, ol_id_input_cb, queue);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, vp, GuiLayerFullscreen);

    /* Block here until any key is pressed */
    InputEvent event;
    furi_message_queue_get(queue, &event, FuriWaitForever);

    /* Clean up in reverse order */
    gui_remove_view_port(gui, vp);
    furi_record_close(RECORD_GUI);
    view_port_free(vp);
    furi_message_queue_free(queue);

    return 0;
}
