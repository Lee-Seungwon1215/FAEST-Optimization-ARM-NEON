/*
 *  SPDX-License-Identifier: MIT
 *
 *  OWF (One-Way Function) ARM64 NEON Implementation for FAEST
 *  Uses parallel AES encryption
 */

#include "owf_neon.h"
#include "aes_neon.h"
#include <string.h>

#define IV_SIZE 16

/*
 * owf_128: AES-128-ECB(key, input)
 * Single block encryption
 */
void owf_128_neon(const uint8_t* key, const uint8_t* input, uint8_t* output) {
    aes_round_keys_neon_t rk;
    aes128_keygen_neon(&rk, key);

    uint8_t buf[64] __attribute__((aligned(16)));
    uint8_t out[64] __attribute__((aligned(16)));

    /* Copy input to first block, zero-pad the rest */
    memcpy(buf, input, 16);
    memset(buf + 16, 0, 48);

    AES_Parallel_4PT(buf, out, rk.bytes, rk.Nr);
    memcpy(output, out, 16);
}

/*
 * owf_192: Two AES-192 encryptions in parallel
 * output[0:15] = AES-192(key, input)
 * output[16:31] = AES-192(key, input XOR 0x01)
 */
void owf_192_neon(const uint8_t* key, const uint8_t* input, uint8_t* output) {
    aes_round_keys_neon_t rk;
    aes192_keygen_neon(&rk, key);

    uint8_t buf[64] __attribute__((aligned(16)));
    uint8_t out[64] __attribute__((aligned(16)));

    /* Block 0: input */
    memcpy(buf, input, 16);

    /* Block 1: input XOR 0x01 in first byte */
    memcpy(buf + 16, input, 16);
    buf[16] ^= 0x01;

    /* Zero-pad remaining blocks */
    memset(buf + 32, 0, 32);

    AES_Parallel_4PT(buf, out, rk.bytes, rk.Nr);

    /* Copy both output blocks */
    memcpy(output, out, 32);
}

/*
 * owf_256: Two AES-256 encryptions in parallel
 * output[0:15] = AES-256(key, input)
 * output[16:31] = AES-256(key, input XOR 0x01)
 */
void owf_256_neon(const uint8_t* key, const uint8_t* input, uint8_t* output) {
    aes_round_keys_neon_t rk;
    aes256_keygen_neon(&rk, key);

    uint8_t buf[64] __attribute__((aligned(16)));
    uint8_t out[64] __attribute__((aligned(16)));

    /* Block 0: input */
    memcpy(buf, input, 16);

    /* Block 1: input XOR 0x01 in first byte */
    memcpy(buf + 16, input, 16);
    buf[16] ^= 0x01;

    /* Zero-pad remaining blocks */
    memset(buf + 32, 0, 32);

    AES_Parallel_4PT(buf, out, rk.bytes, rk.Nr);

    /* Copy both output blocks */
    memcpy(output, out, 32);
}

/*
 * owf_em_128: Even-Mansour construction
 * output = AES-128(input, key) XOR key
 */
void owf_em_128_neon(const uint8_t* key, const uint8_t* input, uint8_t* output) {
    aes_round_keys_neon_t rk;
    aes128_keygen_neon(&rk, input);  /* Note: input is used as AES key */

    uint8_t buf[64] __attribute__((aligned(16)));
    uint8_t out[64] __attribute__((aligned(16)));

    /* Encrypt key (used as plaintext) */
    memcpy(buf, key, 16);
    memset(buf + 16, 0, 48);

    AES_Parallel_4PT(buf, out, rk.bytes, rk.Nr);

    /* XOR result with key */
    for (int i = 0; i < 16; i++) {
        output[i] = out[i] ^ key[i];
    }
}
