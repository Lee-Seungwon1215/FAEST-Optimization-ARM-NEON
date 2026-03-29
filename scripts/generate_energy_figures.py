#!/usr/bin/env python3
"""
Generate energy-per-operation figures and tables from M2 powermetrics results.

Inputs:
  - benchmark_energy_M2/energy_results_M2.csv  (measured on M2)

Outputs:
  - figure_energy_sign_m2.pdf   : energy per sign, grouped bar chart
  - figure_energy_tradeoff.pdf  : speedup vs energy trade-off scatter
  - energy_table_m2.txt         : formatted summary table
"""

import csv
import os
import sys
from collections import defaultdict

import matplotlib.pyplot as plt
import numpy as np

# ----------------------------------------------------------------
# RPi4 power estimates from literature (mW, single-threaded active)
# Cortex-A72 @ 1.5 GHz measured ~1.0–1.4 W per core under full load
# [Dayarathna et al., 2016; Ou et al., 2012; RPi4 datasheet]
# Multi-core (4 cores) ≈ total chip power under full load ≈ 3.5–4.5 W
# ----------------------------------------------------------------
RPI4_POWER = {
    "ref":        1100,   # mW, single-core (Cortex-A72 ~1.1 W/core under load)
    "opt-ref":    1100,
    "neon":       1100,
    "neon-pthread": 4000, # mW, 4 cores active under full load
}

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR   = os.path.dirname(SCRIPT_DIR)
CSV_M2     = os.path.join(ROOT_DIR, "results", "energy_m2.csv")

PARAMS_FAEST = ["128s", "128f", "192s", "192f", "256s", "256f"]
PARAMS_EM    = ["em_128s", "em_128f", "em_192s", "em_192f", "em_256s", "em_256f"]
PARAMS_ALL   = PARAMS_FAEST + PARAMS_EM
BUILDS       = ["ref", "opt-ref", "neon", "neon-pthread"]
BUILD_LABELS = ["ref", "opt-ref", "neon", "neon-pthread"]

COLORS  = ["#d62728", "#ff7f0e", "#2ca02c", "#1f77b4"]
HATCHES = ["///", "...", "xxx", ""]


# ----------------------------------------------------------------
# Load M2 CSV
# ----------------------------------------------------------------
def load_m2_csv(path):
    """Returns dict[build][param][op] = {'power_mw': ..., 'time_us': ..., 'energy_uj': ...}"""
    data = defaultdict(lambda: defaultdict(dict))
    if not os.path.exists(path):
        print(f"ERROR: CSV not found: {path}", file=sys.stderr)
        sys.exit(1)

    with open(path) as f:
        reader = csv.DictReader(f)
        for row in reader:
            build = row["build"]
            param = row["param"]
            op    = row["operation"]
            try:
                data[build][param][op] = {
                    "power_mw":  float(row["avg_power_mW"]),
                    "time_us":   float(row["time_us"]),
                    "energy_uj": float(row["energy_uJ"]),
                }
            except (ValueError, KeyError):
                pass
    return data


# ----------------------------------------------------------------
# Also load existing timing data from benchmark_results_M2/ and RPi4/
# to compute RPi4 energy estimates
# ----------------------------------------------------------------
def load_timing_from_bench_results(results_dir, builds_map):
    """
    Parse benchmark_results_{M2,RPi4}/ txt files.
    Returns dict[build][param][op] = time_us (float)
    builds_map: {'ref': 'ref', 'opt-ref': 'opt-ref', ...} mapping internal name -> filename prefix
    """
    import re
    data = defaultdict(lambda: defaultdict(dict))

    for build, prefix in builds_map.items():
        for param in PARAMS_ALL:
            fname = os.path.join(results_dir, f"{prefix}_{param}.txt")
            if not os.path.exists(fname):
                continue
            with open(fname) as f:
                content = f.read()
            lines = content.split("\n")
            for i, line in enumerate(lines):
                stripped = line.strip()
                for op in ("keygen", "sign", "verify"):
                    if stripped.startswith(op) and "100" in stripped and i + 1 < len(lines):
                        nxt = lines[i + 1]
                        m = re.search(r"([\d.]+)\s*(ns|us|ms|s)\b", nxt)
                        if m:
                            val  = float(m.group(1))
                            unit = m.group(2)
                            if unit == "ns": val /= 1000
                            elif unit == "ms": val *= 1000
                            elif unit == "s":  val *= 1e6
                            data[build][param][op] = val
    return data


# ----------------------------------------------------------------
# Figure 1: Energy per Sign (M2 measured, RPi4 estimated)
# ----------------------------------------------------------------
def figure_energy_sign(m2_data, rpi4_timing):
    fig, axes = plt.subplots(1, 2, figsize=(14, 5.5))

    for ax_idx, (platform, params_subset, title_suffix) in enumerate([
        ("FAEST",    PARAMS_FAEST, "FAEST"),
        ("FAEST-EM", PARAMS_EM,    "FAEST-EM"),
    ]):
        ax = axes[ax_idx]
        x      = np.arange(len(params_subset))
        width  = 0.18
        offsets = [-1.5, -0.5, 0.5, 1.5]

        for i, build in enumerate(BUILDS):
            m2_energies = []
            for p in params_subset:
                try:
                    m2_energies.append(m2_data[build][p]["sign"]["energy_uj"])
                except KeyError:
                    m2_energies.append(float("nan"))

            ax.bar(x + offsets[i] * width, m2_energies, width,
                   label=BUILD_LABELS[i], color=COLORS[i],
                   hatch=HATCHES[i], edgecolor="black", linewidth=0.5)

        ax.set_yscale("log")
        ax.set_xlabel("Parameter Set", fontsize=11)
        ax.set_ylabel("Energy per Signature (µJ)", fontsize=11)
        ax.set_title(f"Apple M2 — {title_suffix}", fontsize=13, fontweight="bold")
        ax.set_xticks(x)
        ax.set_xticklabels(params_subset, fontsize=9)
        ax.legend(fontsize=8, loc="upper left")
        ax.grid(axis="y", alpha=0.3, linestyle="--")
        ax.set_axisbelow(True)

    fig.suptitle("Energy per Signature (µJ) — Apple M2 (Measured)",
                 fontsize=14, fontweight="bold", y=1.02)
    plt.tight_layout()
    out = os.path.join(ROOT_DIR, "figure_energy_sign_m2.pdf")
    plt.savefig(out, dpi=300, bbox_inches="tight")
    plt.close()
    print(f"Saved: {out}")


# ----------------------------------------------------------------
# Figure 2: Energy per Sign — M2 + RPi4 side by side (both platforms)
# ----------------------------------------------------------------
def figure_energy_both_platforms(m2_data, rpi4_timing):
    """Two rows: FAEST / FAEST-EM; two columns: M2 / RPi4."""
    fig, axes = plt.subplots(2, 2, figsize=(14, 11))

    for row_idx, (params_subset, variant_label) in enumerate([
        (PARAMS_FAEST, "FAEST"),
        (PARAMS_EM,    "FAEST-EM"),
    ]):
        for col_idx, (platform_label, get_energy) in enumerate([
            ("Apple M2 (Measured)", _m2_energy_getter(m2_data)),
            ("Raspberry Pi 4 (Estimated)", _rpi4_energy_getter(rpi4_timing)),
        ]):
            ax = axes[row_idx][col_idx]
            x       = np.arange(len(params_subset))
            width   = 0.18
            offsets = [-1.5, -0.5, 0.5, 1.5]

            for i, build in enumerate(BUILDS):
                energies = [get_energy(build, p, "sign") for p in params_subset]
                ax.bar(x + offsets[i] * width, energies, width,
                       label=BUILD_LABELS[i], color=COLORS[i],
                       hatch=HATCHES[i], edgecolor="black", linewidth=0.5)

            ax.set_yscale("log")
            ax.set_xlabel("Parameter Set", fontsize=10)
            ax.set_ylabel("Energy per Signature (µJ)", fontsize=10)
            ax.set_title(f"{platform_label} — {variant_label}", fontsize=11, fontweight="bold")
            ax.set_xticks(x)
            ax.set_xticklabels(params_subset, fontsize=8)
            ax.legend(fontsize=7, loc="upper left")
            ax.grid(axis="y", alpha=0.3, linestyle="--")
            ax.set_axisbelow(True)

            if col_idx == 1:
                ax.set_title(ax.get_title() + "\n(estimated from lit. TDP)", fontsize=9)

    fig.suptitle("Energy per Signature (µJ): M2 Measured vs RPi4 Estimated",
                 fontsize=13, fontweight="bold", y=1.01)
    plt.tight_layout()
    out = os.path.join(ROOT_DIR, "figure_energy_sign_both.pdf")
    plt.savefig(out, dpi=300, bbox_inches="tight")
    plt.close()
    print(f"Saved: {out}")


def _m2_energy_getter(m2_data):
    def get(build, param, op):
        try:
            return m2_data[build][param][op]["energy_uj"]
        except KeyError:
            return float("nan")
    return get


def _rpi4_energy_getter(rpi4_timing):
    def get(build, param, op):
        try:
            t_us = rpi4_timing[build][param][op]
            p_mw = RPI4_POWER[build]
            # energy_uJ = power_mW * time_us / 1000
            return p_mw * t_us / 1000.0
        except KeyError:
            return float("nan")
    return get


# ----------------------------------------------------------------
# Figure 3: Time-Energy Trade-off (sign, M2)
# ----------------------------------------------------------------
def figure_tradeoff(m2_data):
    fig, ax = plt.subplots(figsize=(8, 6))

    markers = ["o", "s", "^", "D"]
    for i, build in enumerate(BUILDS):
        times    = []
        energies = []
        labels   = []
        for p in PARAMS_ALL:
            try:
                t = m2_data[build][p]["sign"]["time_us"] / 1000  # -> ms
                e = m2_data[build][p]["sign"]["energy_uj"]
                times.append(t)
                energies.append(e)
                labels.append(p)
            except KeyError:
                pass

        sc = ax.scatter(times, energies, label=BUILD_LABELS[i],
                        color=COLORS[i], marker=markers[i], s=60, zorder=3)

    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_xlabel("Signing Time (ms)", fontsize=12)
    ax.set_ylabel("Energy per Signature (µJ)", fontsize=12)
    ax.set_title("Time vs Energy Trade-off (Sign) — Apple M2", fontsize=13, fontweight="bold")
    ax.legend(fontsize=10)
    ax.grid(alpha=0.3, linestyle="--")
    ax.set_axisbelow(True)

    plt.tight_layout()
    out = os.path.join(ROOT_DIR, "figure_energy_tradeoff_m2.pdf")
    plt.savefig(out, dpi=300, bbox_inches="tight")
    plt.close()
    print(f"Saved: {out}")


# ----------------------------------------------------------------
# Text summary table
# ----------------------------------------------------------------
def print_table(m2_data, rpi4_timing, op="sign"):
    get_rpi4 = _rpi4_energy_getter(rpi4_timing)
    get_m2   = _m2_energy_getter(m2_data)

    lines = []
    lines.append("=" * 110)
    lines.append(f"  Energy per {op.upper()} (µJ)")
    lines.append("=" * 110)
    hdr = f"{'Param':<12}"
    for b in BUILD_LABELS:
        hdr += f"  {'M2:'+b:>18}"
    for b in BUILD_LABELS:
        hdr += f"  {'RPi4:'+b:>18}"
    lines.append(hdr)
    lines.append("-" * 110)

    for p in PARAMS_ALL:
        row = f"{p:<12}"
        for b in BUILDS:
            v = get_m2(b, p, op)
            row += f"  {v:>18.2f}" if not (v != v) else f"  {'N/A':>18}"
        for b in BUILDS:
            v = get_rpi4(b, p, op)
            row += f"  {v:>18.2f}" if not (v != v) else f"  {'N/A':>18}"
        lines.append(row)

    lines.append("=" * 110)
    lines.append("")
    lines.append("RPi4 values are ESTIMATES: power_mW × time_us / 1000")
    lines.append("  ref/opt-ref/neon: 1100 mW (Cortex-A72 single-core active load)")
    lines.append("  neon-pthread:     4000 mW (BCM2711 all 4 cores under full load)")
    lines.append("  See: Ou et al. (2012), Dayarathna et al. (2016)")

    text = "\n".join(lines)
    print(text)
    out = os.path.join(ROOT_DIR, "energy_table_m2.txt")
    with open(out, "w") as f:
        f.write(text + "\n")
    print(f"\nTable written to: {out}")


# ----------------------------------------------------------------
# Main
# ----------------------------------------------------------------
def main():
    print("Loading M2 energy CSV ...")
    m2_data = load_m2_csv(CSV_M2)

    print("Loading RPi4 timing data ...")
    rpi4_dir = os.path.join(ROOT_DIR, "benchmark_results_RPi4")
    rpi4_builds_map = {
        "ref":         "ref",
        "opt-ref":     "opt-ref",
        "neon":        "neon",
        "neon-pthread": "neon-pthread",
    }
    rpi4_timing = load_timing_from_bench_results(rpi4_dir, rpi4_builds_map)

    print("Generating figures ...")
    figure_energy_sign(m2_data, rpi4_timing)
    figure_energy_both_platforms(m2_data, rpi4_timing)
    figure_tradeoff(m2_data)

    print("\nEnergy summary table:")
    print_table(m2_data, rpi4_timing, op="sign")
    print_table(m2_data, rpi4_timing, op="verify")

    print("\nDone.")


if __name__ == "__main__":
    main()
