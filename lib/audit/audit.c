#include "audit.h"

#include <furi.h>
#include <furi_hal_rtc.h>
#include <storage/storage.h>
#include <flipper_format/flipper_format.h>
#include <datetime/datetime.h>

#define TAG "Audit"

static const char* SETTINGS_HEADER = "Audit Settings";
static const uint32_t SETTINGS_VERSION = 1;

static FuriMutex* g_mutex = NULL;
static bool g_settings_loaded = false;
static bool g_enabled = false;

static void audit_ensure_mutex(void) {
    if(g_mutex == NULL) {
        FuriMutex* m = furi_mutex_alloc(FuriMutexTypeNormal);
        if(!__sync_bool_compare_and_swap(&g_mutex, NULL, m)) {
            furi_mutex_free(m);
        }
    }
}

static void audit_load_settings_locked(void) {
    if(g_settings_loaded) return;
    g_settings_loaded = true;
    g_enabled = false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(storage);
    FuriString* header = furi_string_alloc();

    do {
        if(!flipper_format_file_open_existing(ff, AUDIT_SETTINGS_PATH)) break;
        uint32_t version = 0;
        if(!flipper_format_read_header(ff, header, &version)) break;
        if(version != SETTINGS_VERSION) break;
        bool enabled = false;
        if(!flipper_format_read_bool(ff, "enabled", &enabled, 1)) break;
        g_enabled = enabled;
    } while(false);

    furi_string_free(header);
    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);
}

static void audit_save_settings_locked(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, AUDIT_BASE_PATH);
    FlipperFormat* ff = flipper_format_file_alloc(storage);

    do {
        if(!flipper_format_file_open_always(ff, AUDIT_SETTINGS_PATH)) break;
        if(!flipper_format_write_header_cstr(ff, SETTINGS_HEADER, SETTINGS_VERSION)) break;
        bool enabled = g_enabled;
        if(!flipper_format_write_bool(ff, "enabled", &enabled, 1)) break;
    } while(false);

    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);
}

bool audit_is_enabled(void) {
    audit_ensure_mutex();
    furi_mutex_acquire(g_mutex, FuriWaitForever);
    audit_load_settings_locked();
    bool enabled = g_enabled;
    furi_mutex_release(g_mutex);
    return enabled;
}

void audit_set_enabled(bool enabled) {
    audit_ensure_mutex();
    furi_mutex_acquire(g_mutex, FuriWaitForever);
    audit_load_settings_locked();
    g_enabled = enabled;
    audit_save_settings_locked();
    furi_mutex_release(g_mutex);
}

static void audit_sanitize_field(const char* in, char* out, size_t out_size) {
    if(out_size == 0) return;
    size_t n = 0;
    for(size_t i = 0; in && in[i] != '\0' && n < out_size - 1; i++) {
        char c = in[i];
        if(c == ',' || c == '\n' || c == '\r') c = ' ';
        out[n++] = c;
    }
    out[n] = '\0';
}

static void audit_build_log_path(FuriString* out, const DateTime* dt) {
    furi_string_printf(
        out,
        AUDIT_BASE_PATH "/audit-%04u-%02u-%02u.csv",
        dt->year,
        dt->month,
        dt->day);
}

void audit_log_event(
    const char* subsystem,
    const char* operation,
    const char* identifier,
    const char* details) {
    if(!subsystem) subsystem = "";
    if(!operation) operation = "";
    if(!identifier) identifier = "";
    if(!details) details = "";

    audit_ensure_mutex();
    furi_mutex_acquire(g_mutex, FuriWaitForever);
    audit_load_settings_locked();
    if(!g_enabled) {
        furi_mutex_release(g_mutex);
        return;
    }

    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);

    char subsys_buf[AUDIT_MAX_FIELD_LEN];
    char op_buf[AUDIT_MAX_FIELD_LEN];
    char id_buf[AUDIT_MAX_FIELD_LEN];
    char det_buf[AUDIT_MAX_FIELD_LEN];
    audit_sanitize_field(subsystem, subsys_buf, sizeof(subsys_buf));
    audit_sanitize_field(operation, op_buf, sizeof(op_buf));
    audit_sanitize_field(identifier, id_buf, sizeof(id_buf));
    audit_sanitize_field(details, det_buf, sizeof(det_buf));

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, AUDIT_BASE_PATH);

    FuriString* path = furi_string_alloc();
    audit_build_log_path(path, &dt);

    File* file = storage_file_alloc(storage);
    bool new_file = !storage_file_exists(storage, furi_string_get_cstr(path));

    if(storage_file_open(
           file, furi_string_get_cstr(path), FSAM_WRITE, FSOM_OPEN_APPEND)) {
        if(new_file) {
            const char* header = "timestamp,subsystem,operation,identifier,details\n";
            storage_file_write(file, header, strlen(header));
        }
        char line[256];
        int n = snprintf(
            line,
            sizeof(line),
            "%04u-%02u-%02u %02u:%02u:%02u,%s,%s,%s,%s\n",
            dt.year,
            dt.month,
            dt.day,
            dt.hour,
            dt.minute,
            dt.second,
            subsys_buf,
            op_buf,
            id_buf,
            det_buf);
        if(n > 0) {
            storage_file_write(file, line, (size_t)n);
        }
        storage_file_close(file);
    } else {
        FURI_LOG_W(TAG, "Failed to open %s", furi_string_get_cstr(path));
    }
    storage_file_free(file);
    furi_string_free(path);
    furi_record_close(RECORD_STORAGE);

    furi_mutex_release(g_mutex);
}
