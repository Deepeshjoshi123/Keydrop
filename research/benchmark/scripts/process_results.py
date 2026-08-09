#!/usr/bin/env python3
"""Aggregate raw Keydrop benchmark CSV files into processed summaries."""

from __future__ import annotations

import csv
import statistics
from collections import defaultdict
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
RAW_DIR = ROOT / "research" / "benchmark" / "raw_data"
PROCESSED_DIR = ROOT / "research" / "benchmark" / "processed"


FORMAT_METRICS = [
    "packet_size_bytes",
    "encode_latency_ns",
    "decode_latency_ns",
    "allocations",
    "allocated_bytes",
    "throughput_per_sec",
]

STREAM_METRICS = [
    "messages",
    "decoded_messages",
    "emitted_packets",
    "emitted_bytes",
    "throughput_msg_per_sec",
    "avg_latency_us_per_message",
]


def read_csvs(pattern: str) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    for path in sorted(RAW_DIR.glob(pattern)):
        with path.open(newline="", encoding="utf-8") as handle:
            rows.extend(csv.DictReader(handle))
    return rows


def aggregate(rows: list[dict[str, str]], group_key: str, metrics: list[str]) -> list[dict[str, object]]:
    grouped: dict[str, list[dict[str, str]]] = defaultdict(list)
    for row in rows:
        grouped[row[group_key]].append(row)

    out: list[dict[str, object]] = []
    for group, group_rows in sorted(grouped.items()):
        summary: dict[str, object] = {group_key: group, "samples": len(group_rows)}
        for metric in metrics:
            values = [float(row[metric]) for row in group_rows if row.get(metric) not in ("", None)]
            if not values:
                continue
            summary[f"{metric}_mean"] = statistics.fmean(values)
            summary[f"{metric}_stdev"] = statistics.stdev(values) if len(values) > 1 else 0.0
            summary[f"{metric}_min"] = min(values)
            summary[f"{metric}_max"] = max(values)
        out.append(summary)
    return out


def write_csv(path: Path, rows: list[dict[str, object]]) -> None:
    if not rows:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    fieldnames = list(rows[0].keys())
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)
    print(f"wrote {path}")


def main() -> int:
    format_rows = read_csvs("format_benchmark_*.csv")
    stream_rows = read_csvs("stream_optimizer_*.csv")

    if not format_rows and not stream_rows:
        raise SystemExit(f"no raw benchmark CSV files found in {RAW_DIR}")

    if format_rows:
        write_csv(
            PROCESSED_DIR / "format_summary.csv",
            aggregate(format_rows, "format", FORMAT_METRICS),
        )

    if stream_rows:
        for row in stream_rows:
            row["benchmark"] = "stream_optimizer"
        write_csv(
            PROCESSED_DIR / "stream_optimizer_summary.csv",
            aggregate(stream_rows, "benchmark", STREAM_METRICS),
        )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
