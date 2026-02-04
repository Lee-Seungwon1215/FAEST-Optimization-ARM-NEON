/*
 *  SPDX-License-Identifier: MIT
 *
 *  PRG (Pseudo-Random Generator) ARM64 NEON Interface for FAEST
 *  Uses parallel AES-CTR mode for high throughput
 */

#ifndef PRG_NEON_H
#define PRG_NEON_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * PRG using AES in CTR mode with NEON parallel encryption
 *
 * key: AES key (16, 24, or 32 bytes depending on security level)
 * iv: Initial IV (16 bytes), will be modified internally
 * tweak: Value to add to upper word of IV
 * out: Output buffer
 * outlen: Number of bytes to generate
 *
 * The IV is incremented little-endian on the first 4 bytes (matching FAEST reference)
 */
void prg_neon_128(const uint8_t* key, const uint8_t* iv, uint32_t tweak,
                  uint8_t* out, size_t outlen);
void prg_neon_192(const uint8_t* key, const uint8_t* iv, uint32_t tweak,
                  uint8_t* out, size_t outlen);
void prg_neon_256(const uint8_t* key, const uint8_t* iv, uint32_t tweak,
                  uint8_t* out, size_t outlen);

/*
 * Fixed-output PRG functions
 * prg_2_lambda: outputs 2*lambda bits (32 bytes for 128, 48 for 192, 64 for 256)
 * prg_4_lambda: outputs 4*lambda bits (64 bytes for 128, 96 for 192, 128 for 256)
 */
void prg_2_lambda_neon_128(const uint8_t* key, const uint8_t* iv, uint32_t tweak,
                           uint8_t* out);
void prg_2_lambda_neon_192(const uint8_t* key, const uint8_t* iv, uint32_t tweak,
                           uint8_t* out);
void prg_2_lambda_neon_256(const uint8_t* key, const uint8_t* iv, uint32_t tweak,
                           uint8_t* out);

void prg_4_lambda_neon_128(const uint8_t* key, const uint8_t* iv, uint32_t tweak,
                           uint8_t* out);
void prg_4_lambda_neon_192(const uint8_t* key, const uint8_t* iv, uint32_t tweak,
                           uint8_t* out);
void prg_4_lambda_neon_256(const uint8_t* key, const uint8_t* iv, uint32_t tweak,
                           uint8_t* out);

#ifdef __cplusplus
}
#endif

#endif /* PRG_NEON_H */
