/**
 * @file overlork_dashboard_scene_totp.c
 * @brief TOTP scene — RFC 6238 (HMAC-SHA256) 6-digit codes, 30s window.
 *
 * Reads a base32-encoded seed from /ext/overlork/totp.key on the SD card,
 * decodes it, and refreshes the displayed code every second via a FuriTimer.
 * The bottom of the screen shows a progress bar that drains as the current
 * 30-second window expires.
 *
 * Layout (128 x 64 px):
 *   y= 4   "TOTP"                 FontPrimary    centered
 *   y=30   "123456"               FontBigNumbers centered
 *   y=52   [====      ] (6px h)   progress bar, 120px wide
 *
 * Notes:
 *   - HMAC-SHA256 variant of RFC 6238 (TOTP-SHA256) — matches the
 *     `algorithm=SHA256` parameter of an otpauth:// URI.
 *   - Uses the Flipper's RTC as-is. If the device clock is local time,
 *     codes will be offset by that timezone.
 *   - Key is zeroed from RAM on scene exit.
 */

#include "../overlork_dashboard_i.h"
#include <overlork_crypto/overlork_crypto.h>
#include <furi_hal_rtc.h>
#include <storage/storage.h>
#include <datetime/datetime.h>
#include <string.h>

#define OD_TOTP_PATH         EXT_PATH("overlork/totp.key")
#define OD_TOTP_PERIOD_SEC   30u
#define OD_TOTP_KEY_FILE_MAX 256u /* upper bound on raw key file bytes */

typedef struct {
    char    code[8]; /* "000000\0" or "------\0" */
    uint8_t remaining; /* seconds remaining in current window (0..30) */
    bool    no_key; /* true when key file is missing or invalid */
} TotpModel;

/* ── Base32 (RFC 4648, padding tolerated) ───────────────────────────────── */

static int od_base32_decode(const char* in, size_t in_len, uint8_t* out, size_t out_max) {
    uint32_t buffer = 0;
    int      bits   = 0;
    size_t   n      = 0;

    for(size_t i = 0; i < in_len; i++) {
        char c = in[i];
        int  v;
        if(c >= 'A' && c <= 'Z') {
            v = c - 'A';
        } else if(c >= 'a' && c <= 'z') {
            v = c - 'a';
        } else if(c >= '2' && c <= '7') {
            v = c - '2' + 26;
        } else if(c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '=') {
            continue;
        } else {
            return -1;
        }
        buffer = (buffer << 5) | (uint32_t)v;
        bits += 5;
        if(bits >= 8) {
            if(n >= out_max) return -1;
            bits -= 8;
            out[n++] = (uint8_t)((buffer >> bits) & 0xFFu);
        }
    }
    return (int)n;
}

/* ── TOTP (RFC 6238 with HMAC-SHA256) ───────────────────────────────────── */

static uint32_t od_totp_compute(const uint8_t* key, size_t key_len, uint64_t counter) {
    uint8_t msg[8];
    for(int i = 7; i >= 0; i--) {
        msg[i] = (uint8_t)(counter & 0xFFu);
        counter >>= 8;
    }
    uint8_t mac[OL_CRYPTO_SHA256_LEN];
    if(ol_crypto_hmac_sha256(key, key_len, msg, sizeof(msg), mac) != OlCryptoOk) {
        return 0;
    }
    /* RFC 4226 §5.3 dynamic truncation */
    uint8_t  offset = mac[OL_CRYPTO_SHA256_LEN - 1] & 0x0Fu;
    uint32_t bin    = ((uint32_t)(mac[offset] & 0x7Fu) << 24) |
                   ((uint32_t)mac[offset + 1] << 16) |
                   ((uint32_t)mac[offset + 2] << 8) |
                   ((uint32_t)mac[offset + 3]);
    return bin % 1000000u;
}

/* ── Key loading ────────────────────────────────────────────────────────── */

static bool od_totp_load_key(uint8_t* key, uint8_t* key_len) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    bool     ok      = false;

    if(storage_file_exists(storage, OD_TOTP_PATH)) {
        File* f = storage_file_alloc(storage);
        if(storage_file_open(f, OD_TOTP_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
            char     raw[OD_TOTP_KEY_FILE_MAX];
            uint16_t got = storage_file_read(f, raw, sizeof(raw));
            storage_file_close(f);
            if(got > 0) {
                int n = od_base32_decode(raw, got, key, OD_TOTP_KEY_MAX);
                if(n > 0) {
                    *key_len = (uint8_t)n;
                    ok       = true;
                }
            }
        }
        storage_file_free(f);
    }

    furi_record_close(RECORD_STORAGE);
    return ok;
}

/* ── Time helper ────────────────────────────────────────────────────────── */

static uint32_t od_totp_now(void) {
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    return datetime_datetime_to_timestamp(&dt);
}

/* ── View callbacks ─────────────────────────────────────────────────────── */

static void od_totp_draw_callback(Canvas* canvas, void* _model) {
    TotpModel* model = _model;
    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 4, AlignCenter, AlignTop, "TOTP");

    if(model->no_key) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignCenter, "No key file");
        canvas_draw_str_aligned(
            canvas, 64, 42, AlignCenter, AlignCenter, "/ext/overlork/totp.key");
        return;
    }

    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignCenter, model->code);

    const uint8_t bar_x = 4;
    const uint8_t bar_y = 52;
    const uint8_t bar_w = 120;
    const uint8_t bar_h = 6;
    canvas_draw_frame(canvas, bar_x, bar_y, bar_w, bar_h);
    uint8_t fill = (uint8_t)((uint16_t)(bar_w - 2) * model->remaining / OD_TOTP_PERIOD_SEC);
    if(fill > 0) canvas_draw_box(canvas, bar_x + 1, bar_y + 1, fill, bar_h - 2);
}

static bool od_totp_input_callback(InputEvent* event, void* context) {
    UNUSED(event);
    UNUSED(context);
    /* Back is handled by the dispatcher's navigation callback. */
    return false;
}

/* ── Refresh ────────────────────────────────────────────────────────────── */

static void od_totp_refresh(OdApp* app) {
    if(app->totp_no_key) {
        with_view_model(
            app->totp_view,
            TotpModel * model,
            {
                model->no_key    = true;
                model->remaining = 0;
                strncpy(model->code, "------", sizeof(model->code));
                model->code[sizeof(model->code) - 1] = '\0';
            },
            true);
        return;
    }

    uint32_t now       = od_totp_now();
    uint64_t counter   = (uint64_t)now / OD_TOTP_PERIOD_SEC;
    uint32_t code_int  = od_totp_compute(app->totp_key, app->totp_key_len, counter);
    uint8_t  remaining = (uint8_t)(OD_TOTP_PERIOD_SEC - (now % OD_TOTP_PERIOD_SEC));

    with_view_model(
        app->totp_view,
        TotpModel * model,
        {
            model->no_key    = false;
            model->remaining = remaining;
            snprintf(model->code, sizeof(model->code), "%06lu", (unsigned long)code_int);
        },
        true);
}

static void od_totp_timer_callback(void* context) {
    OdApp* app = context;
    od_totp_refresh(app);
}

/* ── Scene handlers ─────────────────────────────────────────────────────── */

void overlork_dashboard_scene_totp_on_enter(void* context) {
    OdApp* app = context;

    view_set_context(app->totp_view, app);
    view_set_draw_callback(app->totp_view, od_totp_draw_callback);
    view_set_input_callback(app->totp_view, od_totp_input_callback);
    view_allocate_model(app->totp_view, ViewModelTypeLocking, sizeof(TotpModel));

    memset(app->totp_key, 0, sizeof(app->totp_key));
    app->totp_key_len = 0;
    app->totp_no_key  = !od_totp_load_key(app->totp_key, &app->totp_key_len);

    /* Render the first code immediately so the view doesn't flash blank */
    od_totp_refresh(app);

    app->totp_timer = furi_timer_alloc(od_totp_timer_callback, FuriTimerTypePeriodic, app);
    furi_timer_start(app->totp_timer, furi_kernel_get_tick_frequency());

    view_dispatcher_switch_to_view(app->view_dispatcher, OdViewTotp);
}

bool overlork_dashboard_scene_totp_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void overlork_dashboard_scene_totp_on_exit(void* context) {
    OdApp* app = context;

    if(app->totp_timer) {
        furi_timer_stop(app->totp_timer);
        furi_timer_free(app->totp_timer);
        app->totp_timer = NULL;
    }

    /* Wipe key material from RAM before leaving the scene. */
    memset(app->totp_key, 0, sizeof(app->totp_key));
    app->totp_key_len = 0;
    app->totp_no_key  = false;

    view_free_model(app->totp_view);
}
