# FAEST ARM Optimized

ARM 프로세서를 위한 FAEST 포스트-양자 서명 체계의 최적화 구현.

## 프로젝트 구조

```
faest-arm-optimized/
├── faest-ref/           # 레퍼런스 구현 (순수 C)
├── faest-neon/          # ARM NEON SIMD 최적화
├── faest-neon-pthread/  # NEON + pthread 멀티코어 병렬화
├── benchmark.sh         # 벤치마크 스크립트
└── parse_benchmarks.py  # 결과 파싱 도구
```

## 구현 비교

| 구현 | 설명 | 최적화 수준 |
|------|------|------------|
| `faest-ref` | NIST 제출 레퍼런스 코드 | 없음 (baseline) |
| `faest-ref` + OpenSSL | 하드웨어 AES 가속 | HW 가속 |
| `faest-neon` | ARM NEON SIMD (4/8블록 병렬 AES) | SIMD |
| `faest-neon-pthread` | NEON + pthread (tau 라운드 병렬화) | SIMD + 멀티코어 |

## 빌드 방법

### 의존성

- Meson (>= 0.59)
- Ninja
- GCC 또는 Clang (C11 지원)
- OpenSSL (선택사항, HW 가속용)

### 빌드

```bash
# 레퍼런스 구현 (OpenSSL 없이)
cd faest-ref
meson setup build -DSHA3=opt64 -Dopenssl=disabled --buildtype=release
ninja -C build

# 레퍼런스 구현 (OpenSSL 포함)
meson setup build -DSHA3=opt64 -Dopenssl=enabled --buildtype=release
ninja -C build

# NEON 최적화
cd faest-neon
meson setup build -DSHA3=opt64 --buildtype=release
ninja -C build

# NEON + pthread (스레드 수 지정 가능)
cd faest-neon-pthread
meson setup build -DSHA3=opt64 -Dnum-threads=4 --buildtype=release
ninja -C build
```

### 테스트

```bash
ninja -C build test
```

### 벤치마크

```bash
# Catch2 벤치마크
ninja -C build
./build/faest_128s/faest_128s_bench_c2 "[bench]"
```

## 벤치마크 결과

### Apple M2 (4P + 4E cores)

| 구현 | FAEST-128s Sign | FAEST-128s Verify |
|------|-----------------|-------------------|
| ref (baseline) | ~5.6 s | ~4.9 s |
| ref + OpenSSL | ~43 ms | ~38 ms |
| neon | ~44 ms | ~39 ms |
| neon-pthread (4T) | ~35 ms | ~30 ms |

### pthread 스레드 수별 성능 (FAEST-128s)

| Threads | Sign | Verify | 향상 |
|---------|------|--------|------|
| 1 | 44.6 ms | 39.7 ms | 기준 |
| 2 | 38.8 ms | 33.7 ms | +15% |
| 4 | 35.1 ms | 30.4 ms | +21% |
| 8 | 34.5 ms | 29.6 ms | +23% |

## 파라미터 세트

12개의 FAEST 파라미터 세트 지원:

- **FAEST-128s/f**: 128-bit 보안
- **FAEST-192s/f**: 192-bit 보안
- **FAEST-256s/f**: 256-bit 보안
- **FAEST-EM-128s/f/192s/f/256s/f**: Extended Mask 변형

## 타겟 플랫폼

- Apple Silicon (M1/M2/M3)
- Raspberry Pi 4 (Cortex-A72)
- 기타 ARM64 프로세서

## 라이선스

MIT License (SPDX-License-Identifier: MIT)

## 참고

- [FAEST 공식 사이트](https://faest.info/)
- [NIST PQC 표준화](https://csrc.nist.gov/projects/post-quantum-cryptography)
