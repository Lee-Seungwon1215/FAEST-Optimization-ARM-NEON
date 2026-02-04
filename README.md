# FAEST ARM Optimized

ARM 프로세서를 위한 FAEST 포스트-양자 서명 체계의 최적화 구현.

## 프로젝트 구조

```
faest-arm-optimized/
├── faest-ref/           # 레퍼런스 구현 (순수 C, OpenSSL 옵션)
├── faest-neon/          # ARM NEON 최적화 + pthread 병렬화 옵션
├── benchmark.sh         # 벤치마크 스크립트
└── parse_benchmarks.py  # 결과 파싱 도구
```

## 구현 비교

| 구현 | 빌드 옵션 | 설명 |
|------|----------|------|
| ref (baseline) | `-Dopenssl=disabled` | 순수 C 구현 |
| ref + OpenSSL | `-Dopenssl=enabled` | HW AES 가속 |
| neon | (기본) | ARM NEON SIMD (4/8블록 병렬 AES) |
| neon + pthread | `-Dparallel-vole=enabled` | NEON + 멀티코어 병렬화 |

## 빌드 방법

### 의존성

- Meson (>= 0.59)
- Ninja
- GCC 또는 Clang (C11 지원)
- OpenSSL (선택사항)

### 빌드 예시

```bash
# 레퍼런스 (baseline)
cd faest-ref
meson setup build -DSHA3=opt64 -Dopenssl=disabled --buildtype=release
ninja -C build

# 레퍼런스 + OpenSSL (HW 가속)
meson setup build -DSHA3=opt64 -Dopenssl=enabled --buildtype=release
ninja -C build

# NEON 최적화 (순차)
cd faest-neon
meson setup build -DSHA3=opt64 --buildtype=release
ninja -C build

# NEON + pthread (병렬)
meson setup build -DSHA3=opt64 -Dparallel-vole=enabled -Dnum-threads=4 --buildtype=release
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

### Apple M2 (4P + 4E cores) - FAEST-128s

| 구현 | Sign | Verify |
|------|------|--------|
| ref (baseline) | ~5.6 s | ~4.9 s |
| ref + OpenSSL | ~43 ms | ~38 ms |
| neon | ~44 ms | ~39 ms |
| neon + pthread (4T) | ~35 ms | ~30 ms |

### pthread 스레드 수별 성능

| Threads | Sign | 향상 |
|---------|------|------|
| 1 | 44.6 ms | 기준 |
| 2 | 38.8 ms | +15% |
| 4 | 35.1 ms | +21% |
| 8 | 34.5 ms | +23% |

권장: 성능 코어 수에 맞춰 `-Dnum-threads` 설정

## 지원 파라미터

12개 FAEST 파라미터 세트:
- FAEST-128s/f, 192s/f, 256s/f
- FAEST-EM-128s/f, 192s/f, 256s/f

## 타겟 플랫폼

- Apple Silicon (M1/M2/M3)
- Raspberry Pi 4 (Cortex-A72)
- 기타 ARM64 프로세서

## 라이선스

MIT License

## 참고

- [FAEST 공식 사이트](https://faest.info/)
- [NIST PQC 표준화](https://csrc.nist.gov/projects/post-quantum-cryptography)
