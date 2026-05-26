/**
 * @file overlork_crypto.h
 * @brief OverL0rk cryptographic utilities for Flipper Zero FAPs.
 *
 * Provides a minimal, safe API over the Flipper's hardware primitives:
 *
 *  - TRNG (True Random Number Generator) via furi_hal_random_*
 *  - SHA-256 hashing via mbedtls_sha256_*
 *  - HMAC-SHA256 message authentication via mbedtls_md_hmac_*
 *  - AES-256-GCM authenticated encryption via furi_hal_crypto_gcm_*
 *
 * ⚠  The Flipper Zero was NOT designed as a secure device.  Keys loaded
 * into RAM can be recovered with a debugger.  This library is intended
 * for lightweight integrity and confidentiality in a benign-environment
 * context — not adversarial crypto.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Sizes ──────────────────────────────────────────────────────────────── */

#define OL_CRYPTO_SHA256_LEN  32u  /**< SHA-256 digest length in bytes    */
#define OL_CRYPTO_AES_KEY_LEN 32u  /**< AES-256 key length in bytes       */
#define OL_CRYPTO_GCM_IV_LEN  12u  /**< GCM initialisation vector (bytes) */
#define OL_CRYPTO_GCM_TAG_LEN 16u  /**< GCM authentication tag (bytes)    */

/* ── Status ─────────────────────────────────────────────────────────────── */

typedef enum {
    OlCryptoOk          = 0, /**< Operation succeeded             */
    OlCryptoErrParam    = 1, /**< NULL or invalid parameter       */
    OlCryptoErrHash     = 2, /**< mbedtls SHA-256 / HMAC error    */
    OlCryptoErrAes      = 3, /**< AES-GCM engine error            */
    OlCryptoErrAuthFail = 4, /**< GCM tag mismatch (decrypt)      */
} OlCryptoStatus;

/* ── TRNG ───────────────────────────────────────────────────────────────── */

/**
 * Fill @p buf with @p len cryptographically random bytes from the hardware
 * TRNG (STM32WB RNG peripheral).
 *
 * @param[out] buf  Destination buffer.
 * @param[in]  len  Number of bytes to generate.
 */
void ol_crypto_random_bytes(uint8_t* buf, size_t len);

/**
 * Return a 32-bit random value from the hardware TRNG.
 */
uint32_t ol_crypto_random_u32(void);

/**
 * Generate a fresh GCM initialisation vector (12 random bytes).
 *
 * @param[out] iv  12-byte output buffer.
 */
void ol_crypto_random_iv(uint8_t iv[OL_CRYPTO_GCM_IV_LEN]);

/* ── SHA-256 ─────────────────────────────────────────────────────────────  */

/**
 * Compute the SHA-256 digest of @p data.
 *
 * @param[in]  data    Input data.
 * @param[in]  len     Input length in bytes.
 * @param[out] digest  32-byte output buffer.
 *
 * @return OlCryptoOk on success, OlCryptoErrParam / OlCryptoErrHash on failure.
 */
OlCryptoStatus ol_crypto_sha256(
    const uint8_t* data,
    size_t         len,
    uint8_t        digest[OL_CRYPTO_SHA256_LEN]);

/* ── HMAC-SHA256 ─────────────────────────────────────────────────────────── */

/**
 * Compute HMAC-SHA256 of @p data authenticated with @p key.
 *
 * @param[in]  key      HMAC key bytes.
 * @param[in]  key_len  Key length in bytes.
 * @param[in]  data     Input data.
 * @param[in]  data_len Input length in bytes.
 * @param[out] mac      32-byte output buffer.
 *
 * @return OlCryptoOk on success.
 */
OlCryptoStatus ol_crypto_hmac_sha256(
    const uint8_t* key,
    size_t         key_len,
    const uint8_t* data,
    size_t         data_len,
    uint8_t        mac[OL_CRYPTO_SHA256_LEN]);

/* ── AES-256-GCM ─────────────────────────────────────────────────────────── */

/**
 * Encrypt @p plaintext using AES-256-GCM and produce an authentication tag.
 *
 * @param[in]  key         32-byte AES-256 key.
 * @param[in]  iv          12-byte initialisation vector (must be unique per
 *                         (key, message) pair — use ol_crypto_random_iv()).
 * @param[in]  aad         Additional authenticated data (may be NULL).
 * @param[in]  aad_len     Length of AAD (0 if aad is NULL).
 * @param[in]  plaintext   Input plaintext.
 * @param[out] ciphertext  Output ciphertext (same length as plaintext).
 * @param[in]  len         Plaintext/ciphertext length in bytes.
 * @param[out] tag         16-byte authentication tag.
 *
 * @return OlCryptoOk on success.
 */
OlCryptoStatus ol_crypto_aes_gcm_encrypt(
    const uint8_t  key[OL_CRYPTO_AES_KEY_LEN],
    const uint8_t  iv[OL_CRYPTO_GCM_IV_LEN],
    const uint8_t* aad,
    size_t         aad_len,
    const uint8_t* plaintext,
    uint8_t*       ciphertext,
    size_t         len,
    uint8_t        tag[OL_CRYPTO_GCM_TAG_LEN]);

/**
 * Decrypt @p ciphertext using AES-256-GCM and verify the authentication tag.
 *
 * @param[in]  key         32-byte AES-256 key.
 * @param[in]  iv          12-byte initialisation vector (same as used for enc).
 * @param[in]  aad         Additional authenticated data (same as used for enc).
 * @param[in]  aad_len     Length of AAD.
 * @param[in]  ciphertext  Input ciphertext.
 * @param[out] plaintext   Output plaintext (same length as ciphertext).
 * @param[in]  len         Ciphertext/plaintext length in bytes.
 * @param[in]  tag         16-byte authentication tag to verify.
 *
 * @return OlCryptoOk on success, OlCryptoErrAuthFail if tag doesn't match.
 */
OlCryptoStatus ol_crypto_aes_gcm_decrypt(
    const uint8_t  key[OL_CRYPTO_AES_KEY_LEN],
    const uint8_t  iv[OL_CRYPTO_GCM_IV_LEN],
    const uint8_t* aad,
    size_t         aad_len,
    const uint8_t* ciphertext,
    uint8_t*       plaintext,
    size_t         len,
    const uint8_t  tag[OL_CRYPTO_GCM_TAG_LEN]);

#ifdef __cplusplus
}
#endif
