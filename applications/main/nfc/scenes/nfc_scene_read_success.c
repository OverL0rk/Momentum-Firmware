#include "../helpers/protocol_support/nfc_protocol_support.h"
#include "../nfc_app_i.h"
#include <audit/audit.h>

static void nfc_scene_read_success_audit(NfcApp* nfc) {
    NfcProtocol protocol = nfc_device_get_protocol(nfc->nfc_device);
    if(protocol == NfcProtocolInvalid) return;
    size_t uid_len = 0;
    const uint8_t* uid = nfc_device_get_uid(nfc->nfc_device, &uid_len);
    char uid_str[33] = {0};
    for(size_t i = 0; i < uid_len && i < 16; i++) {
        snprintf(uid_str + i * 2, sizeof(uid_str) - i * 2, "%02X", uid[i]);
    }
    const char* proto_name = nfc_device_get_protocol_name(protocol);
    audit_log_event("NFC", "READ", uid_str, proto_name ? proto_name : "Unknown");
}

void nfc_scene_read_success_on_enter(void* context) {
    nfc_scene_read_success_audit(context);
    nfc_protocol_support_on_enter(NfcProtocolSupportSceneReadSuccess, context);
}

bool nfc_scene_read_success_on_event(void* context, SceneManagerEvent event) {
    return nfc_protocol_support_on_event(NfcProtocolSupportSceneReadSuccess, context, event);
}

void nfc_scene_read_success_on_exit(void* context) {
    nfc_protocol_support_on_exit(NfcProtocolSupportSceneReadSuccess, context);
}
