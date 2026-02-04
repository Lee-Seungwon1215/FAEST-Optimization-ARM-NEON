# FAEST ARM Optimized

ARM 프로세서를 위한 FAEST 포스트-양자 서명 체계의 최적화 구현.

## 프로젝트 구조

```
faest-arm-optimized/
├── faest-ref/           # 레퍼런스 구현 (순수 C, OpenSSL 옵션)
├── faest-neon/          # ARM NEON 최적화 (Apple Silicon용, macOS 구문)
├── faest-neon-RPi4/     # ARM NEON 최적화 (Raspberry Pi 4용, GNU 구문)
├── benchmark.sh         # 벤치마크 스크립트
└── parse_benchmarks.py  # 결과 파싱 도구
```

## 구현 비교

| 구현 | 빌드 옵션 | 설명 |
|------|----------|------|
| ref (baseline) | `-Dopenssl=disabled` | 순수 C 구현 |
| ref + OpenSSL | `-Dopenssl=enabled` | 최적화된 SW AES (HW 가속 지원 시) |
| neon | (기본) | ARM NEON SIMD (4/8블록 병렬 AES) |
| neon + pthread | `-Dparallel-vole=enabled` | NEON + 멀티코어 병렬화 |

## 빌드 방법

### 의존성

- Meson (>= 0.57)
- Ninja
- GCC 또는 Clang (C11 지원)
- OpenSSL (선택사항)
- Boost (테스트용)

### 빌드 예시

```bash
# 레퍼런스 (baseline)
cd faest-ref
meson setup build -Dopenssl=disabled --buildtype=release
ninja -C build

# 레퍼런스 + OpenSSL
meson setup build -Dopenssl=enabled --buildtype=release
ninja -C build

# NEON 최적화 (Raspberry Pi 4)
cd faest-neon-RPi4
meson setup build --buildtype=release
ninja -C build

# NEON + pthread (권장)
meson setup build -Dparallel-vole=enabled -Dnum-threads=4 --buildtype=release
ninja -C build
```

### 테스트

```bash
ninja -C build test
```

### 벤치마크

```bash
# Catch2 벤치마크 (빌드 시 -Dbenchmarks=enabled -Dcatch2=enabled 필요)
./build/faest_128s/faest_128s_bench_c2 "[bench]"
```

## 성능 결과

### Raspberry Pi 4 (Cortex-A72, 4 cores) - FAEST-128s

| 구현 | keygen | Sign | Verify | Speedup |
|------|--------|------|--------|---------|
| ref (순수 C) | 59.19 µs | 21.90 s | 21.86 s | 1x |
| ref + OpenSSL | 4.11 µs | 497 ms | 482 ms | ~44x |
| **neon-RPi4** | 2.74 µs | 407 ms | 390 ms | **~54x** |

### Raspberry Pi 4 - FAEST-128f

| 구현 | keygen | Sign | Verify |
|------|--------|------|--------|
| neon (순차) | 2.83 µs | 85.6 ms | 73.8 ms |
| **neon + pthread (4T)** | 2.77 µs | 72.2 ms | 62.5 ms |

pthread 병렬화로 **+18-19% 성능 향상**

### Apple M2 (4P + 4E cores) - FAEST-128s

| 구현 | Sign | Verify |
|------|------|--------|
| ref (baseline) | ~5.6 s | ~4.9 s |
| ref + OpenSSL | ~43 ms | ~38 ms |
| neon | ~44 ms | ~39 ms |
| neon + pthread (4T) | ~35 ms | ~30 ms |

## 지원 파라미터

12개 FAEST 파라미터 세트:
- FAEST-128s/f, 192s/f, 256s/f
- FAEST-EM-128s/f, 192s/f, 256s/f

## 타겟 플랫폼

| 플랫폼 | 구현 | 비고 |
|--------|------|------|
| Apple Silicon (M1/M2/M3) | faest-neon | macOS 어셈블러 구문 |
| Raspberry Pi 4 (Cortex-A72) | faest-neon-RPi4 | GNU 어셈블러 구문 |
| 기타 ARM64 Linux | faest-neon-RPi4 | GNU 어셈블러 구문 |

## 라이선스

MIT License

## 참고

- [FAEST 공식 사이트](https://faest.info/)
- [NIST PQC 표준화](https://csrc.nist.gov/projects/post-quantum-cryptography)
