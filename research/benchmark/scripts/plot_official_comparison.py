#!/usr/bin/env python3
"""Phase 7 official-comparison figures.

Reads the exact raw official_trials.csv of a manifest study (no data
changes: per-format/per-workload trial means) and renders:

  official_packet_bytes.{png,svg,pdf}   — packet bytes per format × workload
  official_bandwidth_reduction.{...}    — byte reduction vs JSON (%)
  official_latency.{...}                — encode + decode latency (small multiples)

Palette: validated categorical slots from the repository design tokens
(see scripts/validate_palette.js)."""

from __future__ import annotations

import argparse
import csv
import statistics
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# Validated categorical palette (light surface #fcfcfb).
COLORS = {
    "json_nlohmann": "#eb6834",
    "protobuf": "#2a78d6",
    "msgpack_official": "#1baf7a",
    "keydrop_stateless": "#e87ba4",
    "keydrop_stateful": "#008300",
}
LABELS = {
    "json_nlohmann": "JSON (nlohmann 3.11.3)",
    "protobuf": "Protobuf (3.21.12)",
    "msgpack_official": "MessagePack (4.0.0)",
    "keydrop_stateless": "Keydrop stateless",
    "keydrop_stateful": "Keydrop stateful (steady)",
}
FORMAT_ORDER = ["json_nlohmann", "protobuf", "msgpack_official", "keydrop_stateless", "keydrop_stateful"]

SURFACE = "#fcfcfb"
INK = "#0b0b0b"
MUTED = "#52514e"
GRID = "#d9d8d2"


def style_axes(ax) -> None:
    ax.set_facecolor(SURFACE)
    for spine in ("top", "right"):
        ax.spines[spine].set_visible(False)
    for spine in ("left", "bottom"):
        ax.spines[spine].set_color(GRID)
    ax.tick_params(colors=MUTED, length=0)
    ax.grid(axis="y", color=GRID, linewidth=1, zorder=0)
    ax.set_axisbelow(True)


def read_trials(csv_path: Path) -> dict[tuple[str, str], list[dict[str, str]]]:
    grouped: dict[tuple[str, str], list[dict[str, str]]] = {}
    with csv_path.open(newline="", encoding="utf-8") as handle:
        for row in csv.DictReader(handle):
            if not row.get("format") or not row.get("workload"):
                continue
            grouped.setdefault((row["format"], row["workload"]), []).append(row)
    return grouped


def mean(values: list[float]) -> float:
    return statistics.fmean(values) if values else 0.0


def load_metrics(csv_path: Path) -> dict[tuple[str, str], dict[str, float]]:
    """Exact study data: per (format, workload) means over all trials."""
    metrics: dict[tuple[str, str], dict[str, float]] = {}
    for (fmt, workload), rows in read_trials(csv_path).items():
        entry: dict[str, float] = {
            "packet_bytes": mean([float(r["packet_bytes"]) for r in rows if r.get("packet_bytes")]),
            "steady_bytes": mean([float(r["steady_state_avg_bytes"]) for r in rows if r.get("steady_state_avg_bytes")]),
            "encode_ns": mean([float(r["avg_encode_ns"]) for r in rows if r.get("avg_encode_ns")]),
            "decode_ns": mean([float(r["avg_decode_ns"]) for r in rows if r.get("avg_decode_ns")]),
            "throughput": mean([float(r["throughput_per_sec"]) for r in rows if r.get("throughput_per_sec")]),
        }
        metrics[(fmt, workload)] = entry
    return metrics


def effective_bytes(entry: dict[str, float]) -> float:
    return entry["steady_bytes"] if entry["steady_bytes"] else entry["packet_bytes"]


def figure_packet_bytes(metrics, out_dir: Path) -> None:
    workloads = ["W1_fixed_record", "W3_string_heavy", "W4_timestamp_stream"]
    workload_labels = ["W1 · fixed record", "W3 · string-heavy", "W4 · timestamp stream\n(steady-state)"]
    formats = [f for f in FORMAT_ORDER if any((f, w) in metrics for w in workloads)]

    fig, ax = plt.subplots(figsize=(9.5, 5.2), facecolor=SURFACE)
    fig.patch.set_facecolor(SURFACE)
    width = 0.16
    positions = range(len(workloads))

    for slot, fmt in enumerate(formats):
        values = []
        for w in workloads:
            entry = metrics.get((fmt, w))
            values.append(effective_bytes(entry) if entry else 0.0)
        x = [p + (slot - (len(formats) - 1) / 2) * width for p in positions]
        bars = ax.bar(x, values, width=width - 0.02, color=COLORS[fmt], label=LABELS[fmt], zorder=3)
        for bar, value in zip(bars, values):
            if value > 0:
                ax.text(
                    bar.get_x() + bar.get_width() / 2,
                    bar.get_height() + 0.8,
                    f"{value:.1f}",
                    ha="center",
                    va="bottom",
                    fontsize=8.5,
                    color=INK,
                )

    ax.set_xticks(list(positions))
    ax.set_xticklabels(workload_labels, fontsize=9.5, color=INK)
    ax.set_ylabel("bytes per record", fontsize=9.5, color=MUTED)
    ax.set_ylim(0, 88)
    ax.legend(frameon=False, fontsize=8.5, loc="upper left", ncol=2)
    style_axes(ax)
    fig.tight_layout()
    for ext in ("png", "svg", "pdf"):
        fig.savefig(out_dir / f"official_packet_bytes.{ext}", dpi=200, facecolor=SURFACE)
    plt.close(fig)


def figure_bandwidth_reduction(metrics, out_dir: Path) -> None:
    rows = []
    for workload, label in [
        ("W1_fixed_record", "W1 · fixed record"),
        ("W3_string_heavy", "W3 · string-heavy"),
        ("W4_timestamp_stream", "W4 · timestamp stream"),
    ]:
        json_entry = metrics.get(("json_nlohmann", workload))
        if not json_entry:
            continue
        baseline = effective_bytes(json_entry)
        for fmt in FORMAT_ORDER:
            if fmt == "json_nlohmann":
                continue
            entry = metrics.get((fmt, workload))
            if not entry or effective_bytes(entry) <= 0:
                continue
            rows.append((label, LABELS[fmt], 100.0 * (baseline - effective_bytes(entry)) / baseline, COLORS[fmt]))

    fig, ax = plt.subplots(figsize=(9.5, 4.6), facecolor=SURFACE)
    fig.patch.set_facecolor(SURFACE)
    group_labels = sorted({r[0] for r in rows})
    formats = [f for f in FORMAT_ORDER if f != "json_nlohmann" and any(r[1] == LABELS[f] for r in rows)]
    width = 0.24
    positions = range(len(group_labels))

    for slot, fmt in enumerate(formats):
        values = []
        for w in group_labels:
            match = [r for r in rows if r[0] == w and r[1] == LABELS[fmt]]
            values.append(match[0][2] if match else 0.0)
        x = [p + (slot - (len(formats) - 1) / 2) * width for p in positions]
        bars = ax.bar(x, values, width=width - 0.02, color=COLORS[fmt], label=LABELS[fmt], zorder=3)
        for bar, value in zip(bars, values):
            if value != 0:
                ax.text(
                    bar.get_x() + bar.get_width() / 2,
                    bar.get_height() + 1.2,
                    f"{value:.1f}%",
                    ha="center",
                    va="bottom",
                    fontsize=8.5,
                    color=INK,
                )

    ax.axhline(0, color=GRID, linewidth=1)
    ax.set_xticks(list(positions))
    ax.set_xticklabels(group_labels, fontsize=9.5, color=INK)
    ax.set_ylabel("byte reduction vs JSON (%)", fontsize=9.5, color=MUTED)
    ax.set_ylim(0, 100)
    ax.legend(frameon=False, fontsize=8.5, loc="upper left", ncol=2)
    style_axes(ax)
    fig.tight_layout()
    for ext in ("png", "svg", "pdf"):
        fig.savefig(out_dir / f"official_bandwidth_reduction.{ext}", dpi=200, facecolor=SURFACE)
    plt.close(fig)


def figure_latency(metrics, out_dir: Path) -> None:
    workloads = ["W1_fixed_record", "W3_string_heavy"]
    workload_labels = ["W1 · fixed record", "W3 · string-heavy"]
    formats = [f for f in FORMAT_ORDER if any((f, w) in metrics for w in workloads)]

    fig, axes = plt.subplots(1, 2, figsize=(10.5, 4.4), facecolor=SURFACE)
    fig.patch.set_facecolor(SURFACE)
    width = 0.16
    positions = range(len(workloads))

    for ax, metric, title in [
        (axes[0], "encode_ns", "encode latency (ns)"),
        (axes[1], "decode_ns", "decode latency (ns)"),
    ]:
        for slot, fmt in enumerate(formats):
            values = []
            for w in workloads:
                entry = metrics.get((fmt, w))
                values.append(entry[metric] if entry else 0.0)
            x = [p + (slot - (len(formats) - 1) / 2) * width for p in positions]
            ax.bar(x, values, width=width - 0.02, color=COLORS[fmt], label=LABELS[fmt], zorder=3)
        ax.set_xticks(list(positions))
        ax.set_xticklabels(workload_labels, fontsize=9.5, color=INK)
        ax.set_ylabel(title, fontsize=9.5, color=MUTED)
        style_axes(ax)

    axes[0].legend(frameon=False, fontsize=8.5, loc="upper left", ncol=2)
    fig.tight_layout()
    for ext in ("png", "svg", "pdf"):
        fig.savefig(out_dir / f"official_latency.{ext}", dpi=200, facecolor=SURFACE)
    plt.close(fig)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--csv", type=Path, help="official_trials.csv path (default: newest study)")
    parser.add_argument("--out-dir", type=Path, default=ROOT / "research" / "benchmark" / "graphs")
    args = parser.parse_args()

    csv_path = args.csv
    if csv_path is None:
        studies = ROOT / "research" / "benchmark" / "studies"
        candidates = sorted(studies.glob("*/raw/official_trials.csv"), key=lambda p: p.stat().st_mtime)
        if not candidates:
            raise SystemExit("no official_trials.csv found in any study")
        csv_path = candidates[-1]

    metrics = load_metrics(csv_path)
    out_dir = args.out_dir.resolve()
    out_dir.mkdir(parents=True, exist_ok=True)
    figure_packet_bytes(metrics, out_dir)
    figure_bandwidth_reduction(metrics, out_dir)
    figure_latency(metrics, out_dir)
    print(f"figures written to {out_dir} (source: {csv_path})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
