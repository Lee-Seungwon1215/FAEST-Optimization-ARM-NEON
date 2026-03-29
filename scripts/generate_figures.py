#!/usr/bin/env python3
"""
Generate performance comparison figures from results CSV files.

Reads:
  results/m2_performance.csv
  results/rpi4_performance.csv

Outputs:
  figure1_faest_sign.pdf      - FAEST sign performance (M2 + RPi4)
  figure2_faest_em_sign.pdf   - FAEST-EM sign performance (M2 + RPi4)
  figure3_speedup.pdf         - Speedup over ref (M2 + RPi4)

Usage:
  python3 scripts/generate_figures.py
"""

import csv
import os
import sys

import matplotlib.pyplot as plt
import numpy as np

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR   = os.path.dirname(SCRIPT_DIR)

M2_CSV   = os.path.join(ROOT_DIR, "results", "m2_performance.csv")
RPI4_CSV = os.path.join(ROOT_DIR, "results", "rpi4_performance.csv")

PARAMS_FAEST = ['128s', '128f', '192s', '192f', '256s', '256f']
PARAMS_EM    = ['em_128s', 'em_128f', 'em_192s', 'em_192f', 'em_256s', 'em_256f']
LABELS_FAEST = ['128s', '128f', '192s', '192f', '256s', '256f']
LABELS_EM    = ['EM-128s', 'EM-128f', 'EM-192s', 'EM-192f', 'EM-256s', 'EM-256f']

BUILDS       = ['ref', 'opt-ref', 'neon', 'neon-pthread']
BUILD_LABELS = ['ref', 'opt-ref', 'neon', 'neon-pthread']
COLORS       = ['#d62728', '#ff7f0e', '#2ca02c', '#1f77b4']
HATCHES      = ['///', '...', 'xxx', '']


def load_csv(path):
    """
    Returns dict[build][param][operation] = mean_us (float).
    """
    if not os.path.exists(path):
        print(f"WARNING: CSV not found: {path}", file=sys.stderr)
        return {}

    data = {}
    with open(path, newline="", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            b = row["build"]
            p = row["param"]
            op = row["operation"]
            try:
                val = float(row["mean_us"])
            except (ValueError, KeyError):
                continue
            data.setdefault(b, {}).setdefault(p, {})[op] = val
    return data


def get_sign_ms(data, build, params):
    """Return sign times in ms for the given build and param list."""
    return [
        data.get(build, {}).get(p, {}).get("sign", float("nan")) / 1000.0
        for p in params
    ]


def make_sign_figure(m2_data, rpi4_data, params, labels, title, filename):
    """Grouped bar chart (log scale) for sign latency on M2 and RPi4."""
    fig, axes = plt.subplots(1, 2, figsize=(14, 5.5))

    for ax, (platform_name, data) in zip(axes, [("Apple M2", m2_data), ("Raspberry Pi 4", rpi4_data)]):
        if not data:
            ax.set_title(f"{platform_name}\n(no data)", fontsize=13)
            continue

        x = np.arange(len(params))
        width = 0.18
        offsets = [-1.5, -0.5, 0.5, 1.5]

        for i, build in enumerate(BUILDS):
            values = get_sign_ms(data, build, params)
            ax.bar(x + offsets[i] * width, values, width,
                   label=BUILD_LABELS[i], color=COLORS[i],
                   hatch=HATCHES[i], edgecolor='black', linewidth=0.5)

        ax.set_yscale('log')
        ax.set_xlabel('Parameter Set', fontsize=11)
        ax.set_ylabel('Signing Time (ms)', fontsize=11)
        ax.set_title(platform_name, fontsize=13, fontweight='bold')
        ax.set_xticks(x)
        ax.set_xticklabels(labels, fontsize=9)
        ax.legend(fontsize=8, loc='upper left')
        ax.grid(axis='y', alpha=0.3, linestyle='--')
        ax.set_axisbelow(True)

    fig.suptitle(title, fontsize=14, fontweight='bold', y=1.02)
    plt.tight_layout()
    plt.savefig(filename, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"Saved: {filename}")


def make_speedup_figure(m2_data, rpi4_data, filename):
    """Speedup over ref for all 12 parameter sets on M2 and RPi4."""
    all_params = PARAMS_FAEST + PARAMS_EM
    all_labels = ['128s', '128f', '192s', '192f', '256s', '256f',
                  'EM-\n128s', 'EM-\n128f', 'EM-\n192s', 'EM-\n192f', 'EM-\n256s', 'EM-\n256f']

    speedup_builds  = ['opt-ref', 'neon', 'neon-pthread']
    speedup_labels  = ['opt-ref', 'neon', 'neon-pthread']
    speedup_colors  = ['#ff7f0e', '#2ca02c', '#1f77b4']
    speedup_hatches = ['...', 'xxx', '']

    fig, axes = plt.subplots(1, 2, figsize=(14, 5.5))

    for ax, (platform_name, data) in zip(axes, [("Apple M2", m2_data), ("Raspberry Pi 4", rpi4_data)]):
        if not data:
            ax.set_title(f"{platform_name}\n(no data)", fontsize=13)
            continue

        ref_times = get_sign_ms(data, 'ref', all_params)
        x = np.arange(len(all_params))
        width = 0.25
        offsets = [-1, 0, 1]

        for i, build in enumerate(speedup_builds):
            opt_times = get_sign_ms(data, build, all_params)
            speedups = [
                r / o if (o and o == o and r == r) else float("nan")
                for r, o in zip(ref_times, opt_times)
            ]
            bars = ax.bar(x + offsets[i] * width, speedups, width,
                          label=speedup_labels[i], color=speedup_colors[i],
                          hatch=speedup_hatches[i], edgecolor='black', linewidth=0.5)

            for bar, val in zip(bars, speedups):
                if val == val and val > 10:
                    ax.text(bar.get_x() + bar.get_width() / 2., bar.get_height() + 1,
                            f'{val:.0f}x', ha='center', va='bottom', fontsize=5.5, rotation=90)

        ax.set_xlabel('Parameter Set', fontsize=11)
        ax.set_ylabel('Speedup (×)', fontsize=11)
        ax.set_title(platform_name, fontsize=13, fontweight='bold')
        ax.set_xticks(x)
        ax.set_xticklabels(all_labels, fontsize=7.5)
        ax.legend(fontsize=8, loc='upper left')
        ax.grid(axis='y', alpha=0.3, linestyle='--')
        ax.set_axisbelow(True)
        ax.axvline(x=5.5, color='gray', linestyle=':', linewidth=1, alpha=0.7)

    fig.suptitle('Speedup over Reference Implementation (Sign)', fontsize=14, fontweight='bold', y=1.02)
    plt.tight_layout()
    plt.savefig(filename, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"Saved: {filename}")


def main():
    print("Loading CSV data ...")
    m2_data   = load_csv(M2_CSV)
    rpi4_data = load_csv(RPI4_CSV)

    make_sign_figure(
        m2_data, rpi4_data,
        PARAMS_FAEST, LABELS_FAEST,
        'FAEST Sign Performance Comparison',
        os.path.join(ROOT_DIR, 'figure1_faest_sign.pdf'),
    )

    make_sign_figure(
        m2_data, rpi4_data,
        PARAMS_EM, LABELS_EM,
        'FAEST-EM Sign Performance Comparison',
        os.path.join(ROOT_DIR, 'figure2_faest_em_sign.pdf'),
    )

    make_speedup_figure(
        m2_data, rpi4_data,
        os.path.join(ROOT_DIR, 'figure3_speedup.pdf'),
    )

    print("\nAll figures generated successfully.")


if __name__ == "__main__":
    main()
