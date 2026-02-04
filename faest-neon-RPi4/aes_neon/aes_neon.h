/*
 *  SPDX-License-Identifier: MIT
 *
 *  ARM64 NEON AES Interface for FAEST
 *  Provides parallel AES encryption using NEON SIMD instructions
 */

#ifndef AES_NEON_H
#define AES_NEON_H

#include <stdint.h>
#include <stddef.h>

#if defined(__aarch64__) || defined(_M_ARM64)
#define HAVE_ARM64_NEON 1
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Round key structure for NEON implementation
 * Stores round keys in byte format (big-endian words in sequence)
 * Compatible with AES_Parallel_4PT and AES_Parallel_8PT assembly
 */
typedef struct {
    uint8_t bytes[(14 + 1) * 16];  /* Max AES-256: 14 rounds + initial */
    int Nr;                         /* Number of rounds: 10, 12, or 14 */
} aes_round_keys_neon_t;

/* Key expansion functions - produce byte format round keys */
void aes128_keygen_neon(aes_round_keys_neon_t* keys, const uint8_t* master_key);
void aes192_keygen_neon(aes_round_keys_neon_t* keys, const uint8_t* master_key);
void aes256_keygen_neon(aes_round_keys_neon_t* keys, const uint8_t* master_key);

/* Parallel encryption functions (ARM64 NEON assembly)
 * These are implemented in aes_neon_4pt.s and aes_neon_8pt.s
 *
 * pt: plaintext blocks (64 bytes for 4PT, 128 bytes for 8PT)
 * ct: ciphertext output (same size as pt)
 * rk: round keys in byte format
 * Nr: number of rounds (10, 12, or 14)
 */
void AES_Parallel_4PT(const uint8_t* pt, uint8_t* ct, const uint8_t* rk, int Nr);
void AES_Parallel_8PT(const uint8_t* pt, uint8_t* ct, const uint8_t* rk, int Nr);

/* Single block encryption using parallel implementation (with padding) */
void aes_encrypt_block_neon(const aes_round_keys_neon_t* keys,
                            const uint8_t* plaintext,
                            uint8_t* ciphertext);

/* Multi-block encryption */
void aes_encrypt_blocks_neon(const aes_round_keys_neon_t* keys,
                             const uint8_t* plaintext,
                             uint8_t* ciphertext,
                             size_t num_blocks);

#ifdef __cplusplus
}
#endif

#endif /* AES_NEON_H */
