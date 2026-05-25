#include "../audit_viewer_i.h"
#include <audit/audit.h>
#include <furi_hal_rtc.h>
#include <storage/storage.h>
#include <datetime/datetime.h>

#define LOG_TAIL_BYTES 2048

static void audit_viewer_load_today_tail(FuriString* out) {
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);

    FuriString* path = furi_string_alloc_printf(
        AUDIT_BASE_PATH "/audit-%04u-%02u-%02u.csv", dt.year, dt.month, dt.day);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    furi_string_reset(out);

    if(!storage_file_exists(storage, furi_string_get_cstr(path))) {
        furi_string_set(out, "No log for today yet.\n\nEnable audit and use\nNFC to generate events.");
    } else if(storage_file_open(
                  file, furi_string_get_cstr(path), FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint64_t size = storage_file_size(file);
        uint64_t offset = (size > LOG_TAIL_BYTES) ? (size - LOG_TAIL_BYTES) : 0;
        storage_file_seek(file, (uint32_t)offset, true);
        uint8_t buf[256];
        while(true) {
            uint16_t read = storage_file_read(file, buf, sizeof(buf) - 1);
            if(read == 0) break;
            buf[read] = '\0';
            furi_string_cat_str(out, (const char*)buf);
        }
        storage_file_close(file);
        if(furi_string_empty(out)) {
            furi_string_set(out, "Log file is empty.");
        }
    } else {
        furi_string_set(out, "Could not open log file.");
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    furi_string_free(path);
}

void audit_viewer_scene_log_on_enter(void* context) {
    AuditViewerApp* app = context;
    TextBox* text_box = app->text_box;

    audit_viewer_load_today_tail(app->log_text);

    text_box_reset(text_box);
    text_box_set_font(text_box, TextBoxFontText);
    text_box_set_focus(text_box, TextBoxFocusEnd);
    text_box_set_text(text_box, furi_string_get_cstr(app->log_text));

    view_dispatcher_switch_to_view(app->view_dispatcher, AuditViewerViewTextBox);
}

bool audit_viewer_scene_log_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void audit_viewer_scene_log_on_exit(void* context) {
    AuditViewerApp* app = context;
    text_box_reset(app->text_box);
}
