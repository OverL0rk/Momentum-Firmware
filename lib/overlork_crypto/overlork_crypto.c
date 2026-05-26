/**
 * @file overlork_crypto.c
 * @brief OverL0rk crypto utilities — implementation.
 *
 * Dependencies (linked by the firmware, not as fap_libs):
 *   - furi_hal_random_*       (targets/f7/furi_hal)
 *   - furi_hal_crypto_gcm_*   (targets/f7/furi_hal)
 *   - mbedtls sha256 / md     (lib/mbedtls)
 */

#include "overlork_crypto.h"

#include <furi.h>
#include <furi_hal_random.h>
#include <furi_hal_crypto.h>
#include <mbedtls/sha256.h>
#include <mbedtls/md.h>

/* ── TRNG ───────────────────────────────────────────────────────────────── */

void ol_crypto_random_bytes(uint8_t* buf, size_t len) {
    furi_check(buf != NULL);
    furi_hal_random_fill_buf(buf, (uint32_t)len);
}

uint32_t ol_crypto_random_u32(void) {
    return furi_hal_random_get();
}

void ol_crypto_random_iv(uint8_t iv[OL_CRYPTO_GCM_IV_LEN]) {
    furi_check(iv != NULL);
    furi_hal_random_fill_buf(iv, OL_CRYPTO_GCM_IV_LEN);
}

/* ── SHA-256 ─────────────────────────────────────────────────────────────── */

OlCryptoStatus ol_crypto_sha256(
    const uint8_t* data,
    size_t         len,
    uint8_t        digest[OL_CRYPTO_SHA256_LEN]) {
    if(!data || !digest) return OlCryptoErrParam;

    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);

    int err = mbedtls_sha256_starts(&ctx, 0 /* is224=0 → SHA-256 */);
    if(err == 0) err = mbedtls_sha256_update(&ctx, data, len);
    if(err == 0) err = mbedtls_sha256_finish(&ctx, digest);

    mbedtls_sha256_free(&ctx);
    return (err == 0) ? OlCryptoOk : OlCryptoErrHash;
}

/* ── HMAC-SHA256 ─────────────────────────────────────────────────────────── */

OlCryptoStatus ol_crypto_hmac_sha256(
    const uint8_t* key,
    size_t         key_len,
    const uint8_t* data,
    size_t         data_len,
    uint8_t        mac[OL_CRYPTO_SHA256_LEN]) {
    if(!key || !data || !mac) return OlCryptoErrParam;

    const mbedtls_md_info_t* md_info =
        mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if(!md_info) return OlCryptoErrHash;

    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);

    int err = mbedtls_md_setup(&ctx, md_info, 1 /* hmac=1 */);
    if(err == 0) err = mbedtls_md_hmac_starts(&ctx, key, key_len);
    if(err == 0) err = mbedtls_md_hmac_update(&ctx, data, data_len);
    if(err == 0) err = mbedtls_md_hmac_finish(&ctx, mac);

    mbedtls_md_free(&ctx);
    return (err == 0) ? OlCryptoOk : OlCryptoErrHash;
}

/* ── AES-256-GCM ─────────────────────────────────────────────────────────── */

OlCryptoStatus ol_crypto_aes_gcm_encrypt(
    const uint8_t  key[OL_CRYPTO_AES_KEY_LEN],
    const uint8_t  iv[OL_CRYPTO_GCM_IV_LEN],
    const uint8_t* aad,
    size_t         aad_len,
    const uint8_t* plaintext,
    uint8_t*       ciphertext,
    size_t         len,
    uint8_t        tag[OL_CRYPTO_GCM_TAG_LEN]) {
    if(!key || !iv || !tag) return OlCryptoErrParam;
    if(len > 0 && (!plaintext || !ciphertext)) return OlCryptoErrParam;

    FuriHalCryptoGCMState state = furi_hal_crypto_gcm_encrypt_and_tag(
        key, iv,
        aad, aad_len,
        plaintext, ciphertext, len,
        tag);

    return (state == FuriHalCryptoGCMStateOk) ? OlCryptoOk : OlCryptoErrAes;
}

OlCryptoStatus ol_crypto_aes_gcm_decrypt(
    const uint8_t  key[OL_CRYPTO_AES_KEY_LEN],
    const uint8_t  iv[OL_CRYPTO_GCM_IV_LEN],
    const uint8_t* aad,
    size_t         aad_len,
    const uint8_t* ciphertext,
    uint8_t*       plaintext,
    size_t         len,
    const uint8_t  tag[OL_CRYPTO_GCM_TAG_LEN]) {
    if(!key || !iv || !tag) return OlCryptoErrParam;
    if(len > 0 && (!ciphertext || !plaintext)) return OlCryptoErrParam;

    FuriHalCryptoGCMState state = furi_hal_crypto_gcm_decrypt_and_verify(
        key, iv,
        aad, aad_len,
        ciphertext, plaintext, len,
        tag);

    switch(state) {
    case FuriHalCryptoGCMStateOk:          return OlCryptoOk;
    case FuriHalCryptoGCMStateAuthFailure: return OlCryptoErrAuthFail;
    default:                               return OlCryptoErrAes;
    }
}
