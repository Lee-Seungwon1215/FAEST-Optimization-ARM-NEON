//
//  aes_neon_8pt.s
//  AES 8-Block Parallel Encryption using ARM64 NEON
//  S-box cached in v16-v31 (256 bytes)
//
//  Target: Raspberry Pi 4 (Cortex-A72, no Crypto Extensions)
//  Syntax: GNU/Linux assembler (converted from Apple/macOS)
//
//  Copyright (c) 2024-2025 FAEST Authors
//  SPDX-License-Identifier: MIT
//

.align 4
.data

// AES S-box (256 bytes) - local copy for independent linking
.globl aes_sbox_data_8pt
.hidden aes_sbox_data_8pt
aes_sbox_data_8pt:
    .byte 0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76
    .byte 0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0
    .byte 0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15
    .byte 0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75
    .byte 0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84
    .byte 0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf
    .byte 0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8
    .byte 0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2
    .byte 0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73
    .byte 0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb
    .byte 0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79
    .byte 0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08
    .byte 0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a
    .byte 0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e
    .byte 0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf
    .byte 0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16

// ShiftRows index
.globl shiftrows_idx_8pt
.hidden shiftrows_idx_8pt
shiftrows_idx_8pt:
    .byte 0, 5, 10, 15, 4, 9, 14, 3, 8, 13, 2, 7, 12, 1, 6, 11

// MixColumns rot1 index
rot1_idx_8pt:
    .byte 1, 2, 3, 0, 5, 6, 7, 4, 9, 10, 11, 8, 13, 14, 15, 12

.align 4
.text
.globl AES_Parallel_8PT
.globl _AES_Parallel_8PT

/*
 * x0: plaintext (128 bytes = 8 blocks)
 * x1: ciphertext
 * x2: round keys (byte format)
 * x3: Nr (number of rounds)
 *
 * Register allocation:
 * v0-v7:   8 state blocks
 * v8:      ShiftRows index
 * v9:      rot1 index
 * v10:     0x1b constant
 * v11:     0x40 constant
 * v12-v15: temporaries
 * v16-v31: S-box (256 bytes, fixed)
 */

// SubBytes - S-box in v16-v31
.macro SubBytes8 reg
    sub     v12.16b, \reg\().16b, v11.16b
    tbl     \reg\().16b, {v16.16b, v17.16b, v18.16b, v19.16b}, \reg\().16b
    sub     v13.16b, v12.16b, v11.16b
    tbx     \reg\().16b, {v20.16b, v21.16b, v22.16b, v23.16b}, v12.16b
    sub     v14.16b, v13.16b, v11.16b
    tbx     \reg\().16b, {v24.16b, v25.16b, v26.16b, v27.16b}, v13.16b
    tbx     \reg\().16b, {v28.16b, v29.16b, v30.16b, v31.16b}, v14.16b
.endm

// ShiftRows
.macro ShiftRows8 reg
    tbl     \reg\().16b, {\reg\().16b}, v8.16b
.endm

// MixColumns - uses v12-v15 as temporaries
.macro MixColumns8 reg
    mov     v12.16b, \reg\().16b                // save original s

    tbl     v13.16b, {v12.16b}, v9.16b          // rot1 = s[(j+1)%4]
    eor     v14.16b, v12.16b, v13.16b           // v14 = s ^ rot1

    // Compute t = s ^ rot1 ^ rot2 ^ rot3
    tbl     v15.16b, {v13.16b}, v9.16b          // rot2
    eor     v13.16b, v14.16b, v15.16b           // s ^ rot1 ^ rot2
    tbl     v15.16b, {v15.16b}, v9.16b          // rot3
    eor     v13.16b, v13.16b, v15.16b           // t = s ^ rot1 ^ rot2 ^ rot3

    // xtime(s ^ rot1) = xtime(v14)
    sshr    v15.16b, v14.16b, #7                // 0x00 or 0xFF
    shl     v14.16b, v14.16b, #1
    and     v15.16b, v15.16b, v10.16b           // 0x00 or 0x1b
    eor     v14.16b, v14.16b, v15.16b           // xtime(s ^ rot1)

    // result = s ^ t ^ xtime(s ^ rot1)
    eor     \reg\().16b, v12.16b, v13.16b       // s ^ t
    eor     \reg\().16b, \reg\().16b, v14.16b   // s ^ t ^ xtime
.endm

// AddRoundKey
.macro AddRoundKey8
    ld1     {v12.16b}, [x2], #16
    eor     v0.16b, v0.16b, v12.16b
    eor     v1.16b, v1.16b, v12.16b
    eor     v2.16b, v2.16b, v12.16b
    eor     v3.16b, v3.16b, v12.16b
    eor     v4.16b, v4.16b, v12.16b
    eor     v5.16b, v5.16b, v12.16b
    eor     v6.16b, v6.16b, v12.16b
    eor     v7.16b, v7.16b, v12.16b
.endm

AES_Parallel_8PT:
_AES_Parallel_8PT:
    // Save callee-saved registers (v8-v15) per AArch64 ABI
    sub     sp, sp, #128
    stp     q8, q9, [sp]
    stp     q10, q11, [sp, #32]
    stp     q12, q13, [sp, #64]
    stp     q14, q15, [sp, #96]

    // Load S-box into v16-v31 (use local copy)
    adrp    x4, aes_sbox_data_8pt
    add     x4, x4, :lo12:aes_sbox_data_8pt
    ld1     {v16.16b, v17.16b, v18.16b, v19.16b}, [x4], #64
    ld1     {v20.16b, v21.16b, v22.16b, v23.16b}, [x4], #64
    ld1     {v24.16b, v25.16b, v26.16b, v27.16b}, [x4], #64
    ld1     {v28.16b, v29.16b, v30.16b, v31.16b}, [x4]

    // Load indices (use local copy)
    adrp    x4, shiftrows_idx_8pt
    add     x4, x4, :lo12:shiftrows_idx_8pt
    ld1     {v8.16b}, [x4], #16
    ld1     {v9.16b}, [x4]

    // Constants
    movi    v10.16b, #0x1b
    movi    v11.16b, #0x40

    // Load 8 plaintext blocks
    ld1     {v0.16b, v1.16b, v2.16b, v3.16b}, [x0], #64
    ld1     {v4.16b, v5.16b, v6.16b, v7.16b}, [x0]

    // Initial AddRoundKey
    AddRoundKey8

    sub     x3, x3, #1

.Lmain_loop_8v2:
    SubBytes8 v0
    SubBytes8 v1
    SubBytes8 v2
    SubBytes8 v3
    SubBytes8 v4
    SubBytes8 v5
    SubBytes8 v6
    SubBytes8 v7

    ShiftRows8 v0
    ShiftRows8 v1
    ShiftRows8 v2
    ShiftRows8 v3
    ShiftRows8 v4
    ShiftRows8 v5
    ShiftRows8 v6
    ShiftRows8 v7

    MixColumns8 v0
    MixColumns8 v1
    MixColumns8 v2
    MixColumns8 v3
    MixColumns8 v4
    MixColumns8 v5
    MixColumns8 v6
    MixColumns8 v7

    AddRoundKey8

    subs    x3, x3, #1
    b.ne    .Lmain_loop_8v2

    // Final round
    SubBytes8 v0
    SubBytes8 v1
    SubBytes8 v2
    SubBytes8 v3
    SubBytes8 v4
    SubBytes8 v5
    SubBytes8 v6
    SubBytes8 v7

    ShiftRows8 v0
    ShiftRows8 v1
    ShiftRows8 v2
    ShiftRows8 v3
    ShiftRows8 v4
    ShiftRows8 v5
    ShiftRows8 v6
    ShiftRows8 v7

    AddRoundKey8

    // Store
    st1     {v0.16b, v1.16b, v2.16b, v3.16b}, [x1], #64
    st1     {v4.16b, v5.16b, v6.16b, v7.16b}, [x1]

    // Restore callee-saved registers (v8-v15)
    ldp     q8, q9, [sp]
    ldp     q10, q11, [sp, #32]
    ldp     q12, q13, [sp, #64]
    ldp     q14, q15, [sp, #96]
    add     sp, sp, #128

    ret
