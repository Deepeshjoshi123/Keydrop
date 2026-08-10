#!/usr/bin/env python3
"""Generate IEEE-compliant graphs from processed benchmark CSV files.

Outputs SVG, PNG (300 dpi), and PDF for each chart.
Uses serif fonts, grayscale-compatible hatches, no embedded titles,
and minimal gridlines per IEEE manuscript conventions.
"""

from __future__ import annotations

import csv
import os
from pathlib import Path

import matplotlib
matplotlib.use("Agg")  # non-interactive backend — must precede pyplot import
import matplotlib.pyplot as plt
import numpy as np


ROOT = Path(__file__).resolve().parents[3]
PROCESSED_DIR = ROOT / "research" / "benchmark" / "processed"
GRAPH_DIR = ROOT / "research" / "benchmark" / "graphs"

# (filename_prefix, column, y_label)
GRAPH_SPECS = [
    ("packet_size_comparison",    "packet_size_bytes_mean",          "Packet Size (bytes)"),
    ("encoding_latency",          "encode_latency_ns_mean",         "Encoding Latency (ns)"),
    ("decoding_latency",          "decode_latency_ns_mean",         "Decoding Latency (ns)"),
    ("throughput",                "throughput_per_sec_mean",        "Throughput (ops/s)"),
    ("bandwidth_usage",           "bytes_per_second_mean",          "Bandwidth (bytes/s)"),
    ("memory_allocation_count",   "allocations_mean",               "Allocations"),
    ("memory_usage",              "allocated_bytes_mean",           "Allocated Bytes"),
    ("cpu_usage",                 "cpu_percent_mean",               "CPU Usage (%)"),
    ("optimization_comparison",   "optimization_ratio_mean",        "Optimization Ratio"),
    ("dictionary_reuse_efficiency", "dictionary_reuse_percent_mean","Dictionary Reuse (%)"),
    ("transport_performance",     "transport_throughput_per_sec_mean", "Throughput (packets/s)"),
    ("scaling_performance",       "scaling_throughput_per_sec_mean",   "Throughput (ops/s)"),
]

# Grayscale colors + hatches for B&W-printable bars
BAR_STYLES = [
    {"color": "#d9d9d9", "hatch": "//"},
    {"color": "#bdbdbd", "hatch": "\\\\"},
    {"color": "#969696", "hatch": "xx"},
    {"color": "#636363", "hatch": ".."},
    {"color": "#bdbdbd", "hatch": "||"},
    {"color": "#d9d9d9", "hatch": "--"},
]


def _setup_ieee_style() -> None:
    plt.rcParams.update({
        "font.family": "serif",
        "font.serif": ["Times New Roman", "DejaVu Serif"],
        "font.size": 9,
        "axes.titlesize": 10,
        "axes.labelsize": 9,
        "xtick.labelsize": 8,
        "ytick.labelsize": 8,
        "legend.fontsize": 8,
        "figure.dpi": 300,
        "savefig.dpi": 300,
        "savefig.bbox": "tight",
        "savefig.pad_inches": 0.05,
    })


def read_format_summary() -> list[dict[str, str]]:
    path = PROCESSED_DIR / "format_summary.csv"
    if not path.exists():
        raise SystemExit(f"missing processed data: {path}")
    with path.open(newline="", encoding="utf-8") as handle:
        return list(csv.DictReader(handle))


def _round_tick_step(max_val: float) -> float:
    """Pick a reasonable tick step so axis labels are round numbers."""
    if max_val <= 0:
        return 1.0
    magnitude = 10 ** np.floor(np.log10(max_val))
    residual = max_val / magnitude
    for candidate in (1, 2, 2.5, 5, 10):
        if candidate * magnitude >= max_val / 4:
            return candidate * magnitude
    return magnitude * 10


def _make_ieee_bar_chart(
    labels: list[str],
    values: list[float],
    y_label: str,
    svg_path: Path,
    png_path: Path,
    pdf_path: Path,
) -> None:
    _setup_ieee_style()

    fig, ax = plt.subplots(figsize=(3.5, 2.6))  # IEEE single-column width
    x = np.arange(len(labels))
    bar_width = 0.55

    bars = []
    for i, (label, value) in enumerate(zip(labels, values)):
        style = BAR_STYLES[i % len(BAR_STYLES)]
        bar = ax.bar(
            x[i], value, bar_width,
            color=style["color"],
            edgecolor="black",
            linewidth=0.5,
            hatch=style["hatch"],
        )
        bars.append(bar)

    # Value labels above each bar — whole numbers for counts/bytes, 1dp for ratios
    for i, (bar_container, v) in enumerate(zip(bars, values)):
        bar = bar_container[0]
        if abs(v) < 1 and v != 0:
            label = f"{v:.2f}"
        elif abs(v) < 100 and v != int(v):
            label = f"{v:.1f}"
        else:
            label = f"{v:.0f}"
        ax.text(
            bar.get_x() + bar.get_width() / 2.0,
            bar.get_height() + max(values) * 0.03,
            label,
            ha="center", va="bottom", fontsize=7,
        )

    ax.set_xticks(x)
    ax.set_xticklabels(labels)
    ax.set_ylabel(y_label)

    # Y-axis: start at 0, round ticks
    y_max = max(values) * 1.18 if values else 1.0
    ax.set_ylim(0, y_max)
    step = _round_tick_step(y_max)
    ax.set_yticks(np.arange(0, y_max + step, step))

    # Faint gridlines
    ax.yaxis.grid(True, linestyle="-", alpha=0.15, color="#888888")
    ax.set_axisbelow(True)

    # Clean spines
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    ax.spines["left"].set_linewidth(0.5)
    ax.spines["bottom"].set_linewidth(0.5)

    fig.tight_layout(pad=0.3)

    svg_path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(str(svg_path), format="svg")
    fig.savefig(str(png_path), format="png")
    fig.savefig(str(pdf_path), format="pdf")
    plt.close(fig)


def main() -> int:
    GRAPH_DIR.mkdir(parents=True, exist_ok=True)
    rows = read_format_summary()
    labels = [row["format"] for row in rows]

    for filename, metric, y_label in GRAPH_SPECS:
        if metric not in rows[0]:
            print(f"skipping {filename}: missing metric {metric}")
            continue
        values = [float(row[metric]) for row in rows]
        _make_ieee_bar_chart(
            labels, values, y_label,
            svg_path=GRAPH_DIR / f"{filename}.svg",
            png_path=GRAPH_DIR / f"{filename}.png",
            pdf_path=GRAPH_DIR / f"{filename}.pdf",
        )
        print(f"wrote {filename}.{{svg,png,pdf}}")

    # Payload reduction relative to JSON
    baseline = next((row for row in rows if row["format"] == "json"), None)
    if baseline is not None:
        baseline_size = float(baseline["packet_size_bytes_mean"])
        reduction_vals = []
        for row in rows:
            size = float(row["packet_size_bytes_mean"])
            reduction = 100.0 * (baseline_size - size) / baseline_size if baseline_size else 0.0
            reduction_vals.append(reduction)
        _make_ieee_bar_chart(
            labels, reduction_vals, "Size Reduction vs JSON (%)",
            svg_path=GRAPH_DIR / "payload_reduction.svg",
            png_path=GRAPH_DIR / "payload_reduction.png",
            pdf_path=GRAPH_DIR / "payload_reduction.pdf",
        )
        print("wrote payload_reduction.{svg,png,pdf}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
