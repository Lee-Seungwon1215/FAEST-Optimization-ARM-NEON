# FAEST-NEON for Raspberry Pi 4

ARM NEON optimized implementation of FAEST post-quantum signature scheme for Raspberry Pi 4 (Cortex-A72).

## Overview

This implementation provides ARM64 NEON SIMD optimizations for the FAEST signature scheme, specifically targeting the Raspberry Pi 4 platform which lacks ARMv8 Crypto Extensions.

### Key Optimizations

- **4-block parallel AES**: Process 4 AES blocks simultaneously using NEON
- **8-block parallel AES**: Process 8 AES blocks simultaneously using NEON
- **S-box table lookup**: 256-byte S-box cached in NEON registers (v16-v31)
- **pthread parallelization**: Multi-core VOLE computation (optional)

### Assembly Syntax

The assembly files (`aes_neon/*.s`) use **GNU/Linux assembler syntax**, converted from the original Apple/macOS syntax for compatibility with Raspberry Pi OS.

## Dependencies

For building:
- `meson` (>= 0.57)
- `ninja`
- `gcc` or `clang` (C11 support)

For tests:
- `libboost-test-dev`

```bash
# Debian/Raspberry Pi OS
sudo apt install meson ninja-build libboost-all-dev
```

## Building

### Basic Build (NEON only)

```bash
meson setup build --buildtype=release
ninja -C build
ninja -C build test
```

### With pthread Parallelization (Recommended)

```bash
meson setup build \
    -Dparallel-vole=enabled \
    -Dnum-threads=4 \
    --buildtype=release
ninja -C build
```

### With Benchmarks

```bash
meson setup build \
    -Dparallel-vole=enabled \
    -Dnum-threads=4 \
    -Dbenchmarks=enabled \
    -Dcatch2=enabled \
    --buildtype=release
ninja -C build

# Run benchmark
./build/faest_128f/faest_128f_bench_c2 '[bench]'
```

## Benchmark Results

### Test Environment
- **Platform**: Raspberry Pi 4 Model B
- **CPU**: ARM Cortex-A72 (4 cores @ 1.5GHz)
- **OS**: Raspberry Pi OS (Debian-based, aarch64)
- **Note**: No ARMv8 Crypto Extensions (no HW AES)

### FAEST-128s Performance

| Implementation | keygen | sign | verify |
|----------------|--------|------|--------|
| ref (pure C) | 59.19 µs | 21.90 s | 21.86 s |
| ref + OpenSSL | 4.11 µs | 497 ms | 482 ms |
| **NEON** | 2.74 µs | 407 ms | 390 ms |

**Speedup vs pure C: ~54x**

### FAEST-128f Performance

| Implementation | keygen | sign | verify |
|----------------|--------|------|--------|
| NEON (sequential) | 2.83 µs | 85.6 ms | 73.8 ms |
| **NEON + pthread (4T)** | 2.77 µs | 72.2 ms | 62.5 ms |

**pthread speedup: ~18-19%**

## Supported Parameters

All 12 FAEST parameter sets are supported:
- FAEST-128s/f, FAEST-192s/f, FAEST-256s/f
- FAEST-EM-128s/f, FAEST-EM-192s/f, FAEST-EM-256s/f

## File Structure

```
faest-neon-RPi4/
├── aes_neon/
│   ├── aes_neon_4pt.s      # 4-block parallel AES (GNU syntax)
│   ├── aes_neon_8pt.s      # 8-block parallel AES (GNU syntax)
│   ├── aes_keygen_neon.c   # AES key expansion
│   ├── owf_neon.c          # One-way function
│   └── prg_neon.c          # Pseudo-random generator
├── sha3/                   # SHA3/Keccak implementation
├── meson.build             # Build configuration
└── README.md
```

## Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `parallel-vole` | disabled | Enable pthread parallelization |
| `num-threads` | 4 | Number of threads for parallel VOLE |
| `SHA3` | opt64 | SHA3 implementation (opt64, armv8a-neon) |
| `openssl` | auto | Use OpenSSL for AES (disabled for NEON) |
| `benchmarks` | disabled | Build benchmark executables |
| `catch2` | disabled | Use Catch2 for benchmarks |

## License

MIT License

## References

- [FAEST Official Site](https://faest.info/)
- [NIST PQC Standardization](https://csrc.nist.gov/projects/post-quantum-cryptography)
- Original implementation: [faest-sign/faest-ref](https://github.com/faest-sign)
