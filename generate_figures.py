import matplotlib.pyplot as plt
import numpy as np

# ============================================================
# Data: Sign performance (all in ms)
# ============================================================

params_faest = ['128s', '128f', '192s', '192f', '256s', '256f']
params_em    = ['EM-128s', 'EM-128f', 'EM-192s', 'EM-192f', 'EM-256s', 'EM-256f']

# --- M2 FAEST Sign (ms) ---
m2_faest = {
    'ref':          [5429.57, 747.489, 19778.9, 2484.72, 43279.5, 5103.29],
    'opt_ref':      [43.8703, 16.0477, 120.795, 49.8986, 186.606, 70.3411],
    'neon':         [43.8623, 16.0009, 133.124, 51.6233, 235.751, 76.1828],
    'neon_pthread': [34.5856, 14.8757, 107.166, 48.6166, 172.794, 69.2244],
}

# --- M2 FAEST-EM Sign (ms) ---
m2_em = {
    'ref':          [4103.72, 568.38, 17412.4, 1859.56, 36479.3, 4296.79],
    'opt_ref':      [32.726, 13.3576, 83.4225, 34.9437, 127.96, 61.1639],
    'neon':         [30.1605, 13.0023, 92.3068, 35.9798, 162.994, 65.2278],
    'neon_pthread': [23.3303, 12.2546, 64.3779, 33.6144, 110.507, 59.6772],
}

# --- RPi4 FAEST Sign (ms) ---
rpi4_faest = {
    'ref':          [21787.2, 3000.74, 79387.8, 10091.2, 175693.8, 20776.0],
    'opt_ref':      [489.208, 96.3416, 1778.5, 391.239, 3268.25, 657.179],
    'neon':         [400.046, 84.6639, 1566.6, 361.374, 3004.59, 633.489],
    'neon_pthread': [258.722, 65.0361, 1111.12, 306.34, 2047.61, 512.812],
}
# RPi4 ref 192s = 1.32313 m = 79387.8 ms, 256s = 2.92823 m = 175693.8 ms

# --- RPi4 FAEST-EM Sign (ms) ---
rpi4_em = {
    'ref':          [16507.4, 2278.02, 70066.8, 7428.65, 146979.6, 17411.3],
    'opt_ref':      [380.525, 78.8707, 1250.67, 259.341, 2228.44, 523.241],
    'neon':         [304.415, 68.4961, 1063.71, 238.068, 1958.66, 496.755],
    'neon_pthread': [189.556, 53.1407, 598.715, 191.073, 1073.98, 393.795],
}
# RPi4 ref EM-192s = 1.16778 m = 70066.8 ms, EM-256s = 2.44966 m = 146979.6 ms

builds = ['ref', 'opt_ref', 'neon', 'neon_pthread']
build_labels = ['ref', 'opt_ref', 'neon', 'neon-pthread']
colors = ['#d62728', '#ff7f0e', '#2ca02c', '#1f77b4']
hatches = ['///', '...', 'xxx', '']

# ============================================================
# Helper: grouped bar chart (log scale, two platforms side by side)
# ============================================================
def make_sign_figure(faest_data_m2, faest_data_rpi4, params, title, filename):
    fig, axes = plt.subplots(1, 2, figsize=(14, 5.5))

    platforms = [('Apple M2', faest_data_m2), ('Raspberry Pi 4', faest_data_rpi4)]

    for ax_idx, (platform_name, data) in enumerate(platforms):
        ax = axes[ax_idx]
        x = np.arange(len(params))
        width = 0.18
        offsets = [-1.5, -0.5, 0.5, 1.5]

        for i, build in enumerate(builds):
            values = data[build]
            bars = ax.bar(x + offsets[i] * width, values, width,
                         label=build_labels[i], color=colors[i],
                         hatch=hatches[i], edgecolor='black', linewidth=0.5)

        ax.set_yscale('log')
        ax.set_xlabel('Parameter Set', fontsize=11)
        ax.set_ylabel('Signing Time (ms)', fontsize=11)
        ax.set_title(f'{platform_name}', fontsize=13, fontweight='bold')
        ax.set_xticks(x)
        ax.set_xticklabels(params, fontsize=9)
        ax.legend(fontsize=8, loc='upper left')
        ax.grid(axis='y', alpha=0.3, linestyle='--')
        ax.set_axisbelow(True)

    fig.suptitle(title, fontsize=14, fontweight='bold', y=1.02)
    plt.tight_layout()
    plt.savefig(filename, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"Saved: {filename}")


# ============================================================
# Figure 1: FAEST Sign Performance
# ============================================================
make_sign_figure(
    m2_faest, rpi4_faest, params_faest,
    'FAEST Sign Performance Comparison',
    '/Users/leeseungwon/faest-arm-optimized/figure1_faest_sign.pdf'
)

# ============================================================
# Figure 2: FAEST-EM Sign Performance
# ============================================================
make_sign_figure(
    m2_em, rpi4_em, params_em,
    'FAEST-EM Sign Performance Comparison',
    '/Users/leeseungwon/faest-arm-optimized/figure2_faest_em_sign.pdf'
)

# ============================================================
# Figure 3: Speedup Ratio (ref -> each optimization, sign)
# ============================================================
fig, axes = plt.subplots(1, 2, figsize=(14, 5.5))

# Combine FAEST + FAEST-EM params
all_params = params_faest + params_em
all_params_short = ['128s','128f','192s','192f','256s','256f',
                    'EM-\n128s','EM-\n128f','EM-\n192s','EM-\n192f','EM-\n256s','EM-\n256f']

platforms_data = [
    ('Apple M2', m2_faest, m2_em),
    ('Raspberry Pi 4', rpi4_faest, rpi4_em),
]

speedup_builds = ['opt_ref', 'neon', 'neon_pthread']
speedup_labels = ['opt_ref', 'neon', 'neon-pthread']
speedup_colors = ['#ff7f0e', '#2ca02c', '#1f77b4']
speedup_hatches = ['...', 'xxx', '']

for ax_idx, (platform_name, faest_data, em_data) in enumerate(platforms_data):
    ax = axes[ax_idx]

    # Compute speedups (ref / optimized)
    ref_all = faest_data['ref'] + em_data['ref']

    x = np.arange(len(all_params))
    width = 0.25
    offsets = [-1, 0, 1]

    for i, build in enumerate(speedup_builds):
        opt_all = faest_data[build] + em_data[build]
        speedups = [r / o for r, o in zip(ref_all, opt_all)]

        bars = ax.bar(x + offsets[i] * width, speedups, width,
                     label=speedup_labels[i], color=speedup_colors[i],
                     hatch=speedup_hatches[i], edgecolor='black', linewidth=0.5)

        # Add value labels on top of bars
        for bar, val in zip(bars, speedups):
            if val > 10:
                ax.text(bar.get_x() + bar.get_width()/2., bar.get_height() + 1,
                       f'{val:.0f}x', ha='center', va='bottom', fontsize=5.5, rotation=90)

    ax.set_xlabel('Parameter Set', fontsize=11)
    ax.set_ylabel('Speedup (×)', fontsize=11)
    ax.set_title(f'{platform_name}', fontsize=13, fontweight='bold')
    ax.set_xticks(x)
    ax.set_xticklabels(all_params_short, fontsize=7.5)
    ax.legend(fontsize=8, loc='upper left')
    ax.grid(axis='y', alpha=0.3, linestyle='--')
    ax.set_axisbelow(True)

    # Add a vertical separator between FAEST and FAEST-EM
    ax.axvline(x=5.5, color='gray', linestyle=':', linewidth=1, alpha=0.7)
    ax.text(2.5, ax.get_ylim()[1]*0.95, 'FAEST', ha='center', fontsize=9,
            fontstyle='italic', alpha=0.6)
    ax.text(8.5, ax.get_ylim()[1]*0.95, 'FAEST-EM', ha='center', fontsize=9,
            fontstyle='italic', alpha=0.6)

fig.suptitle('Speedup over Reference Implementation (Sign)', fontsize=14, fontweight='bold', y=1.02)
plt.tight_layout()
plt.savefig('/Users/leeseungwon/faest-arm-optimized/figure3_speedup.pdf', dpi=300, bbox_inches='tight')
plt.close()
print("Saved: figure3_speedup.pdf")

print("\nAll 3 figures generated successfully!")
