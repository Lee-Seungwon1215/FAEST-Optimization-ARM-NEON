#!/usr/bin/env python3
"""
Parse raw Catch2 benchmark output files and generate CSV files in results/.

Expected input directory layout (produced by benchmark.sh or manual runs):
  benchmark_results_M2/{build}_{param}.txt
  benchmark_results_RPi4/{build}_{param}.txt

  where build ∈ {ref, opt-ref, neon, neon-pthread}
    and param ∈ {128s, 128f, 192s, 192f, 256s, 256f,
                 em_128s, em_128f, em_192s, em_192f, em_256s, em_256f}

Outputs:
  results/m2_performance.csv   - Apple M2 keygen/sign/verify (mean_us, stddev_us, ci_low_us, ci_high_us)
  results/rpi4_performance.csv - Raspberry Pi 4 (same schema)
  results/energy_m2.csv        - Apple M2 energy per operation (copied from benchmark_energy_M2/)

Usage:
  python3 scripts/generate_csvs.py
"""

import csv
import os
import re
import shutil

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR   = os.path.dirname(SCRIPT_DIR)

M2_DIR     = os.path.join(ROOT_DIR, "benchmark_results_M2")
RPI4_DIR   = os.path.join(ROOT_DIR, "benchmark_results_RPi4")
ENERGY_SRC = os.path.join(ROOT_DIR, "benchmark_energy_M2", "energy_results_M2.csv")
RESULTS_DIR = os.path.join(ROOT_DIR, "results")

BUILDS = ["ref", "opt-ref", "neon", "neon-pthread", "neon_owf"]
PARAMS = [
    "128s", "128f", "192s", "192f", "256s", "256f",
    "em_128s", "em_128f", "em_192s", "em_192f", "em_256s", "em_256f",
]
OPERATIONS = ["keygen", "sign", "verify"]


def to_us(value: float, unit: str) -> float:
    """Convert a time value to microseconds."""
    unit = unit.strip()
    if unit == "ns":
        return value / 1000.0
    elif unit in ("us", "µs"):
        return value
    elif unit == "ms":
        return value * 1000.0
    elif unit == "s":
        return value * 1_000_000.0
    raise ValueError(f"Unknown unit: {unit!r}")


def parse_catch2_file(filepath: str) -> dict:
    """
    Parse a single Catch2 benchmark output file.

    Catch2 output format (relevant lines):
      keygen                                         100            25     1.6525 ms
                                              660.448 ns      643.6 ns    683.031 ns
                                              98.7232 ns    79.1257 ns    122.337 ns

    Line 1 (benchmark name line): name ... samples ... iterations ... estimated
    Line 2 (statistics line 1):   mean    low_mean    high_mean
    Line 3 (statistics line 2):   std_dev low_std_dev high_std_dev

    Returns: {operation: {mean_us, stddev_us, ci_low_us, ci_high_us}, ...}
    """
    results = {}

    time_re = re.compile(r"([\d.]+)\s*(ns|µs|us|ms|s)\b")

    with open(filepath, encoding="utf-8") as f:
        lines = f.readlines()

    for i, line in enumerate(lines):
        stripped = line.strip()

        for op in OPERATIONS:
            # Match benchmark name line: starts with operation name, contains sample count
            if stripped.startswith(op) and re.search(r"\b100\b", stripped):
                # Next line: mean  ci_low  ci_high
                # Line after: stddev  low_stddev  high_stddev
                if i + 2 >= len(lines):
                    break

                stats_line  = lines[i + 1]
                stddev_line = lines[i + 2]

                stats  = time_re.findall(stats_line)
                stddev = time_re.findall(stddev_line)

                if len(stats) >= 3 and len(stddev) >= 1:
                    results[op] = {
                        "mean_us":    to_us(float(stats[0][0]),  stats[0][1]),
                        "ci_low_us":  to_us(float(stats[1][0]),  stats[1][1]),
                        "ci_high_us": to_us(float(stats[2][0]),  stats[2][1]),
                        "stddev_us":  to_us(float(stddev[0][0]), stddev[0][1]),
                    }
                break

    return results


def collect_records(bench_dir: str, platform: str) -> list[dict]:
    """Collect all records from a benchmark results directory."""
    records = []

    for build in BUILDS:
        for param in PARAMS:
            filepath = os.path.join(bench_dir, f"{build}_{param}.txt")
            if not os.path.exists(filepath):
                continue

            parsed = parse_catch2_file(filepath)
            for op in OPERATIONS:
                if op not in parsed:
                    continue
                r = parsed[op]
                records.append({
                    "platform":   platform,
                    "param":      param,
                    "build":      build,
                    "operation":  op,
                    "mean_us":    r["mean_us"],
                    "stddev_us":  r["stddev_us"],
                    "ci_low_us":  r["ci_low_us"],
                    "ci_high_us": r["ci_high_us"],
                })

    return records


def write_csv(records: list[dict], path: str) -> None:
    fieldnames = ["param", "build", "operation", "mean_us", "stddev_us", "ci_low_us", "ci_high_us"]
    with open(path, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        for r in records:
            writer.writerow({k: r[k] for k in fieldnames})
    print(f"Written: {path}  ({len(records)} rows)")


def main():
    os.makedirs(RESULTS_DIR, exist_ok=True)

    # M2
    if os.path.isdir(M2_DIR):
        m2_records = collect_records(M2_DIR, "M2")
        write_csv(m2_records, os.path.join(RESULTS_DIR, "m2_performance.csv"))
    else:
        print(f"Warning: {M2_DIR} not found — skipping M2 performance CSV")

    # RPi4
    if os.path.isdir(RPI4_DIR):
        rpi4_records = collect_records(RPI4_DIR, "RPi4")
        write_csv(rpi4_records, os.path.join(RESULTS_DIR, "rpi4_performance.csv"))
    else:
        print(f"Warning: {RPI4_DIR} not found — skipping RPi4 performance CSV")

    # Energy
    if os.path.exists(ENERGY_SRC):
        dst = os.path.join(RESULTS_DIR, "energy_m2.csv")
        shutil.copy2(ENERGY_SRC, dst)
        print(f"Copied:  {dst}")
    else:
        print(f"Warning: {ENERGY_SRC} not found — skipping energy CSV")


if __name__ == "__main__":
    main()
