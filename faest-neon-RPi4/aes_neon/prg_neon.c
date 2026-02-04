/*
 *  SPDX-License-Identifier: MIT
 *
 *  PRG (Pseudo-Random Generator) ARM64 NEON Implementation for FAEST
 *  Uses parallel AES-CTR mode for high throughput
 */

#include "prg_neon.h"
#include "aes_neon.h"
#include <string.h>

#define IV_SIZE 16

/*
 * Increment IV little-endian on first 4 bytes (matching FAEST reference)
 * This is the same as faest-ref/aes.c:61-70
 */
static inline void increment_iv_le32(uint8_t* iv) {
    uint32_t iv0;
    memcpy(&iv0, iv, sizeof(uint32_t));
    /* Little-endian increment */
    iv0 = iv0 + 1;  /* Works correctly on little-endian ARM64 */
    memcpy(iv, &iv0, sizeof(uint32_t));
}

/*
 * Add tweak to upper word of IV (last 4 bytes)
 * This is the same as faest-ref/aes.c:307-312
 */
static inline void add_to_upper_word(uint8_t* iv, uint32_t tweak) {
    uint32_t iv3;
    memcpy(&iv3, iv + IV_SIZE - sizeof(uint32_t), sizeof(uint32_t));
    iv3 = iv3 + tweak;  /* Little-endian addition */
    memcpy(iv + IV_SIZE - sizeof(uint32_t), &iv3, sizeof(uint32_t));
}

/*
 * Prepare 8 consecutive IVs for parallel encryption
 * Starting from current_iv, generates IV, IV+1, ..., IV+7
 * Updates current_iv to point to IV+8
 */
static inline void prepare_8_ivs(uint8_t* ivs, uint8_t* current_iv) {
    for (int i = 0; i < 8; i++) {
        memcpy(ivs + i * 16, current_iv, 16);
        increment_iv_le32(current_iv);
    }
}

/*
 * Prepare 4 consecutive IVs for parallel encryption
 */
static inline void prepare_4_ivs(uint8_t* ivs, uint8_t* current_iv) {
    for (int i = 0; i < 4; i++) {
        memcpy(ivs + i * 16, current_iv, 16);
        increment_iv_le32(current_iv);
    }
}

/* ============================================================================
 * AES-128 PRG Functions
 * ============================================================================ */

void prg_neon_128(const uint8_t* key, const uint8_t* iv, uint32_t tweak,
                  uint8_t* out, size_t outlen) {
    aes_round_keys_neon_t rk;
    aes128_keygen_neon(&rk, key);

    uint8_t internal_iv[IV_SIZE];
    memcpy(internal_iv, iv, IV_SIZE);
    add_to_upper_word(internal_iv, tweak);

    uint8_t ivs[128] __attribute__((aligned(16)));

    /* Process 8 blocks (128 bytes) at a time */
    while (outlen >= 128) {
        prepare_8_ivs(ivs, internal_iv);
        AES_Parallel_8PT(ivs, out, rk.bytes, rk.Nr);
        out += 128;
        outlen -= 128;
    }

    /* Process 4 blocks (64 bytes) at a time */
    while (outlen >= 64) {
        prepare_4_ivs(ivs, internal_iv);
        AES_Parallel_4PT(ivs, out, rk.bytes, rk.Nr);
        out += 64;
        outlen -= 64;
    }

    /* Handle remaining full blocks (1-3) */
    if (outlen >= 16) {
        size_t full_blocks = outlen / 16;
        for (size_t i = 0; i < full_blocks; i++) {
            memcpy(ivs + i * 16, internal_iv, 16);
            increment_iv_le32(internal_iv);
        }
        /* Pad remaining slots */
        for (size_t i = full_blocks; i < 4; i++) {
            memset(ivs + i * 16, 0, 16);
        }
        uint8_t tmp[64] __attribute__((aligned(16)));
        AES_Parallel_4PT(ivs, tmp, rk.bytes, rk.Nr);
        memcpy(out, tmp, full_blocks * 16);
        out += full_blocks * 16;
        outlen -= full_blocks * 16;
    }

    /* Handle partial last block */
    if (outlen > 0) {
        memcpy(ivs, internal_iv, 16);
        memset(ivs + 16, 0, 48);
        uint8_t tmp[64] __attribute__((aligned(16)));
        AES_Parallel_4PT(ivs, tmp, rk.bytes, rk.Nr);
        memcpy(out, tmp, outlen);
    }
}

void prg_2_lambda_neon_128(const uint8_t* key, const uint8_t* iv, uint32_t tweak,
                           uint8_t* out) {
    /* 2 * 128 bits = 32 bytes = 2 blocks */
    aes_round_keys_neon_t rk;
    aes128_keygen_neon(&rk, key);

    uint8_t internal_iv[IV_SIZE];
    memcpy(internal_iv, iv, IV_SIZE);
    add_to_upper_word(internal_iv, tweak);

    uint8_t ivs[64] __attribute__((aligned(16)));
    uint8_t tmp[64] __attribute__((aligned(16)));

    /* Prepare 2 IVs, pad rest with zeros */
    memcpy(ivs, internal_iv, 16);
    increment_iv_le32(internal_iv);
    memcpy(ivs + 16, internal_iv, 16);
    memset(ivs + 32, 0, 32);

    AES_Parallel_4PT(ivs, tmp, rk.bytes, rk.Nr);
    memcpy(out, tmp, 32);
}

void prg_4_lambda_neon_128(const uint8_t* key, const uint8_t* iv, uint32_t tweak,
                           uint8_t* out) {
    /* 4 * 128 bits = 64 bytes = 4 blocks */
    aes_round_keys_neon_t rk;
    aes128_keygen_neon(&rk, key);

    uint8_t internal_iv[IV_SIZE];
    memcpy(internal_iv, iv, IV_SIZE);
    add_to_upper_word(internal_iv, tweak);

    uint8_t ivs[64] __attribute__((aligned(16)));

    prepare_4_ivs(ivs, internal_iv);
    AES_Parallel_4PT(ivs, out, rk.bytes, rk.Nr);
}

/* ============================================================================
 * AES-192 PRG Functions
 * ============================================================================ */

void prg_neon_192(const uint8_t* key, const uint8_t* iv, uint32_t tweak,
                  uint8_t* out, size_t outlen) {
    aes_round_keys_neon_t rk;
    aes192_keygen_neon(&rk, key);

    uint8_t internal_iv[IV_SIZE];
    memcpy(internal_iv, iv, IV_SIZE);
    add_to_upper_word(internal_iv, tweak);

    uint8_t ivs[128] __attribute__((aligned(16)));

    /* Process 8 blocks (128 bytes) at a time */
    while (outlen >= 128) {
        prepare_8_ivs(ivs, internal_iv);
        AES_Parallel_8PT(ivs, out, rk.bytes, rk.Nr);
        out += 128;
        outlen -= 128;
    }

    /* Process 4 blocks (64 bytes) at a time */
    while (outlen >= 64) {
        prepare_4_ivs(ivs, internal_iv);
        AES_Parallel_4PT(ivs, out, rk.bytes, rk.Nr);
        out += 64;
        outlen -= 64;
    }

    /* Handle remaining full blocks (1-3) */
    if (outlen >= 16) {
        size_t full_blocks = outlen / 16;
        for (size_t i = 0; i < full_blocks; i++) {
            memcpy(ivs + i * 16, internal_iv, 16);
            increment_iv_le32(internal_iv);
        }
        for (size_t i = full_blocks; i < 4; i++) {
            memset(ivs + i * 16, 0, 16);
        }
        uint8_t tmp[64] __attribute__((aligned(16)));
        AES_Parallel_4PT(ivs, tmp, rk.bytes, rk.Nr);
        memcpy(out, tmp, full_blocks * 16);
        out += full_blocks * 16;
        outlen -= full_blocks * 16;
    }

    /* Handle partial last block */
    if (outlen > 0) {
        memcpy(ivs, internal_iv, 16);
        memset(ivs + 16, 0, 48);
        uint8_t tmp[64] __attribute__((aligned(16)));
        AES_Parallel_4PT(ivs, tmp, rk.bytes, rk.Nr);
        memcpy(out, tmp, outlen);
    }
}

void prg_2_lambda_neon_192(const uint8_t* key, const uint8_t* iv, uint32_t tweak,
                           uint8_t* out) {
    /* 2 * 192 bits = 48 bytes = 3 blocks */
    aes_round_keys_neon_t rk;
    aes192_keygen_neon(&rk, key);

    uint8_t internal_iv[IV_SIZE];
    memcpy(internal_iv, iv, IV_SIZE);
    add_to_upper_word(internal_iv, tweak);

    uint8_t ivs[64] __attribute__((aligned(16)));
    uint8_t tmp[64] __attribute__((aligned(16)));

    /* Prepare 3 IVs, pad rest */
    for (int i = 0; i < 3; i++) {
        memcpy(ivs + i * 16, internal_iv, 16);
        increment_iv_le32(internal_iv);
    }
    memset(ivs + 48, 0, 16);

    AES_Parallel_4PT(ivs, tmp, rk.bytes, rk.Nr);
    memcpy(out, tmp, 48);
}

void prg_4_lambda_neon_192(const uint8_t* key, const uint8_t* iv, uint32_t tweak,
                           uint8_t* out) {
    /* 4 * 192 bits = 96 bytes = 6 blocks */
    aes_round_keys_neon_t rk;
    aes192_keygen_neon(&rk, key);

    uint8_t internal_iv[IV_SIZE];
    memcpy(internal_iv, iv, IV_SIZE);
    add_to_upper_word(internal_iv, tweak);

    uint8_t ivs[128] __attribute__((aligned(16)));
    uint8_t tmp[128] __attribute__((aligned(16)));

    /* Prepare 6 IVs using 8-block buffer, pad rest */
    for (int i = 0; i < 6; i++) {
        memcpy(ivs + i * 16, internal_iv, 16);
        increment_iv_le32(internal_iv);
    }
    memset(ivs + 96, 0, 32);

    AES_Parallel_8PT(ivs, tmp, rk.bytes, rk.Nr);
    memcpy(out, tmp, 96);
}

/* ============================================================================
 * AES-256 PRG Functions
 * ============================================================================ */

void prg_neon_256(const uint8_t* key, const uint8_t* iv, uint32_t tweak,
                  uint8_t* out, size_t outlen) {
    aes_round_keys_neon_t rk;
    aes256_keygen_neon(&rk, key);

    uint8_t internal_iv[IV_SIZE];
    memcpy(internal_iv, iv, IV_SIZE);
    add_to_upper_word(internal_iv, tweak);

    uint8_t ivs[128] __attribute__((aligned(16)));

    /* Process 8 blocks (128 bytes) at a time */
    while (outlen >= 128) {
        prepare_8_ivs(ivs, internal_iv);
        AES_Parallel_8PT(ivs, out, rk.bytes, rk.Nr);
        out += 128;
        outlen -= 128;
    }

    /* Process 4 blocks (64 bytes) at a time */
    while (outlen >= 64) {
        prepare_4_ivs(ivs, internal_iv);
        AES_Parallel_4PT(ivs, out, rk.bytes, rk.Nr);
        out += 64;
        outlen -= 64;
    }

    /* Handle remaining full blocks (1-3) */
    if (outlen >= 16) {
        size_t full_blocks = outlen / 16;
        for (size_t i = 0; i < full_blocks; i++) {
            memcpy(ivs + i * 16, internal_iv, 16);
            increment_iv_le32(internal_iv);
        }
        for (size_t i = full_blocks; i < 4; i++) {
            memset(ivs + i * 16, 0, 16);
        }
        uint8_t tmp[64] __attribute__((aligned(16)));
        AES_Parallel_4PT(ivs, tmp, rk.bytes, rk.Nr);
        memcpy(out, tmp, full_blocks * 16);
        out += full_blocks * 16;
        outlen -= full_blocks * 16;
    }

    /* Handle partial last block */
    if (outlen > 0) {
        memcpy(ivs, internal_iv, 16);
        memset(ivs + 16, 0, 48);
        uint8_t tmp[64] __attribute__((aligned(16)));
        AES_Parallel_4PT(ivs, tmp, rk.bytes, rk.Nr);
        memcpy(out, tmp, outlen);
    }
}

void prg_2_lambda_neon_256(const uint8_t* key, const uint8_t* iv, uint32_t tweak,
                           uint8_t* out) {
    /* 2 * 256 bits = 64 bytes = 4 blocks */
    aes_round_keys_neon_t rk;
    aes256_keygen_neon(&rk, key);

    uint8_t internal_iv[IV_SIZE];
    memcpy(internal_iv, iv, IV_SIZE);
    add_to_upper_word(internal_iv, tweak);

    uint8_t ivs[64] __attribute__((aligned(16)));

    prepare_4_ivs(ivs, internal_iv);
    AES_Parallel_4PT(ivs, out, rk.bytes, rk.Nr);
}

void prg_4_lambda_neon_256(const uint8_t* key, const uint8_t* iv, uint32_t tweak,
                           uint8_t* out) {
    /* 4 * 256 bits = 128 bytes = 8 blocks */
    aes_round_keys_neon_t rk;
    aes256_keygen_neon(&rk, key);

    uint8_t internal_iv[IV_SIZE];
    memcpy(internal_iv, iv, IV_SIZE);
    add_to_upper_word(internal_iv, tweak);

    uint8_t ivs[128] __attribute__((aligned(16)));

    prepare_8_ivs(ivs, internal_iv);
    AES_Parallel_8PT(ivs, out, rk.bytes, rk.Nr);
}
