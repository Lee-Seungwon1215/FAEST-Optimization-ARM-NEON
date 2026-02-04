/*
 *  SPDX-License-Identifier: MIT
 *
 *  OWF (One-Way Function) ARM64 NEON Interface for FAEST
 *  Uses parallel AES encryption
 */

#ifndef OWF_NEON_H
#define OWF_NEON_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * One-Way Functions using AES with NEON optimization
 *
 * owf_128: Single AES-128 block encryption
 * owf_192: Two AES-192 block encryptions (input and input XOR 0x01)
 * owf_256: Two AES-256 block encryptions (input and input XOR 0x01)
 *
 * key: AES key (16, 24, or 32 bytes)
 * input: 16 bytes
 * output: 16 bytes for owf_128, 32 bytes for owf_192/256
 */
void owf_128_neon(const uint8_t* key, const uint8_t* input, uint8_t* output);
void owf_192_neon(const uint8_t* key, const uint8_t* input, uint8_t* output);
void owf_256_neon(const uint8_t* key, const uint8_t* input, uint8_t* output);

/*
 * Even-Mansour OWF variants
 * owf_em_128: AES-128(input, key) XOR key
 */
void owf_em_128_neon(const uint8_t* key, const uint8_t* input, uint8_t* output);

#ifdef __cplusplus
}
#endif

#endif /* OWF_NEON_H */
