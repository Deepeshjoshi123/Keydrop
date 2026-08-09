#!/usr/bin/env python3
"""Generate publication-oriented graphs from processed benchmark CSV files."""

from __future__ import annotations

import csv
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
PROCESSED_DIR = ROOT / "research" / "benchmark" / "processed"
GRAPH_DIR = ROOT / "research" / "benchmark" / "graphs"


GRAPH_SPECS = [
    ("packet_size_comparison", "packet_size_bytes_mean", "Packet Size Comparison", "Bytes"),
    ("encoding_latency", "encode_latency_ns_mean", "Encoding Latency", "Nanoseconds"),
    ("decoding_latency", "decode_latency_ns_mean", "Decoding Latency", "Nanoseconds"),
    ("throughput", "throughput_per_sec_mean", "Throughput", "Operations per second"),
    ("bandwidth_usage", "bytes_per_second_mean", "Bandwidth Usage", "Bytes per second"),
    ("memory_allocation_count", "allocations_mean", "Memory Allocation Count", "Logical allocations"),
    ("memory_usage", "allocated_bytes_mean", "Memory Usage", "Logical allocated bytes"),
    ("cpu_usage", "cpu_percent_mean", "CPU Usage", "Percent"),
    ("optimization_comparison", "optimization_ratio_mean", "Optimization Comparison", "Ratio"),
    ("dictionary_reuse_efficiency", "dictionary_reuse_percent_mean", "Dictionary Reuse Efficiency", "Percent"),
    ("transport_performance", "transport_throughput_per_sec_mean", "Transport Performance", "Packets per second"),
    ("scaling_performance", "scaling_throughput_per_sec_mean", "Scaling Performance", "Operations per second"),
]


def read_format_summary() -> list[dict[str, str]]:
    path = PROCESSED_DIR / "format_summary.csv"
    if not path.exists():
        raise SystemExit(f"missing processed data: {path}")
    with path.open(newline="", encoding="utf-8") as handle:
        return list(csv.DictReader(handle))


def write_svg_bar(path: Path, labels: list[str], values: list[float], title: str, y_label: str) -> None:
    width = 900
    height = 560
    margin_left = 90
    margin_bottom = 90
    margin_top = 70
    plot_width = width - margin_left - 40
    plot_height = height - margin_top - margin_bottom
    max_value = max(values) if values else 1.0
    max_value = max_value if max_value > 0 else 1.0
    bar_gap = 28
    bar_width = max(32, (plot_width - bar_gap * (len(values) + 1)) / max(1, len(values)))
    colors = ["#276FBF", "#F28C28", "#3B8C6E", "#B23A48", "#6D5A8D", "#444444"]

    def x_pos(index: int) -> float:
        return margin_left + bar_gap + index * (bar_width + bar_gap)

    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
        '<rect width="100%" height="100%" fill="white"/>',
        f'<text x="{width / 2}" y="34" text-anchor="middle" font-family="Arial" font-size="22" font-weight="700">{title}</text>',
        f'<text x="24" y="{height / 2}" transform="rotate(-90 24 {height / 2})" text-anchor="middle" font-family="Arial" font-size="14">{y_label}</text>',
        f'<line x1="{margin_left}" y1="{margin_top}" x2="{margin_left}" y2="{height - margin_bottom}" stroke="#222" stroke-width="1.5"/>',
        f'<line x1="{margin_left}" y1="{height - margin_bottom}" x2="{width - 40}" y2="{height - margin_bottom}" stroke="#222" stroke-width="1.5"/>',
    ]

    for tick in range(6):
        value = max_value * tick / 5
        y = height - margin_bottom - (value / max_value) * plot_height
        parts.append(f'<line x1="{margin_left}" y1="{y:.2f}" x2="{width - 40}" y2="{y:.2f}" stroke="#ddd" stroke-width="1"/>')
        parts.append(f'<text x="{margin_left - 10}" y="{y + 4:.2f}" text-anchor="end" font-family="Arial" font-size="11">{value:.2f}</text>')

    for index, (label, value) in enumerate(zip(labels, values)):
        bar_height = (value / max_value) * plot_height
        x = x_pos(index)
        y = height - margin_bottom - bar_height
        parts.append(f'<rect x="{x:.2f}" y="{y:.2f}" width="{bar_width:.2f}" height="{bar_height:.2f}" fill="{colors[index % len(colors)]}"/>')
        parts.append(f'<text x="{x + bar_width / 2:.2f}" y="{y - 8:.2f}" text-anchor="middle" font-family="Arial" font-size="11">{value:.2f}</text>')
        parts.append(f'<text x="{x + bar_width / 2:.2f}" y="{height - margin_bottom + 24}" text-anchor="middle" font-family="Arial" font-size="12">{label}</text>')

    parts.append("</svg>")
    path.write_text("\n".join(parts), encoding="utf-8")


def maybe_write_png(path: Path, labels: list[str], values: list[float], title: str, y_label: str) -> None:
    try:
        import matplotlib.pyplot as plt  # type: ignore
    except Exception:
        return

    fig, ax = plt.subplots(figsize=(8, 5), dpi=200)
    ax.bar(labels, values, color=["#276FBF", "#F28C28", "#3B8C6E", "#B23A48", "#6D5A8D"][: len(labels)])
    ax.set_title(title)
    ax.set_ylabel(y_label)
    ax.grid(axis="y", alpha=0.25)
    fig.tight_layout()
    fig.savefig(path)
    plt.close(fig)


def main() -> int:
    GRAPH_DIR.mkdir(parents=True, exist_ok=True)
    rows = read_format_summary()
    labels = [row["format"] for row in rows]

    for filename, metric, title, y_label in GRAPH_SPECS:
        if metric not in rows[0]:
            print(f"skipping {filename}: missing metric {metric}")
            continue
        values = [float(row[metric]) for row in rows]
        svg_path = GRAPH_DIR / f"{filename}.svg"
        png_path = GRAPH_DIR / f"{filename}.png"
        write_svg_bar(svg_path, labels, values, title, y_label)
        maybe_write_png(png_path, labels, values, title, y_label)
        print(f"wrote {svg_path}")

    reduction_rows = []
    baseline = next((row for row in rows if row["format"] == "json"), None)
    if baseline is not None:
        baseline_size = float(baseline["packet_size_bytes_mean"])
        for row in rows:
            size = float(row["packet_size_bytes_mean"])
            reduction = 100.0 * (baseline_size - size) / baseline_size if baseline_size else 0.0
            reduction_rows.append((row["format"], reduction))
        write_svg_bar(
            GRAPH_DIR / "payload_reduction.svg",
            [item[0] for item in reduction_rows],
            [item[1] for item in reduction_rows],
            "Payload Reduction Relative to JSON",
            "Reduction (%)",
        )
        maybe_write_png(
            GRAPH_DIR / "payload_reduction.png",
            [item[0] for item in reduction_rows],
            [item[1] for item in reduction_rows],
            "Payload Reduction Relative to JSON",
            "Reduction (%)",
        )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
