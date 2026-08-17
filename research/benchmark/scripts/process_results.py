#!/usr/bin/env python3
"""Aggregate one immutable Keydrop benchmark study into summaries."""

from __future__ import annotations

import csv
import hashlib
import json
import statistics
from collections import defaultdict
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
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


def read_csv(path: Path) -> list[dict[str, str]]:
    if not path.exists():
        return []
    with path.open(newline="", encoding="utf-8") as handle:
        return list(csv.DictReader(handle))


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


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
    import argparse

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--study", required=True, type=Path)
    args = parser.parse_args()

    study_dir = args.study.resolve()
    manifest_path = study_dir / "manifest.json"
    if not manifest_path.exists():
        parser.error(f"study manifest does not exist: {manifest_path}")

    raw_dir = study_dir / "raw"
    processed_dir = study_dir / "processed"
    format_path = raw_dir / "format_trials.csv"
    stream_path = raw_dir / "stream_trials.csv"
    format_rows = read_csv(format_path)
    stream_rows = read_csv(stream_path)

    if not format_rows and not stream_rows:
        raise SystemExit(f"no raw benchmark CSV files found in {raw_dir}")

    if format_rows:
        write_csv(
            processed_dir / "format_summary.csv",
            aggregate(format_rows, "format", FORMAT_METRICS),
        )

    if stream_rows:
        for row in stream_rows:
            row["benchmark"] = "stream_optimizer"
        write_csv(
            processed_dir / "stream_optimizer_summary.csv",
            aggregate(stream_rows, "benchmark", STREAM_METRICS),
        )

    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    provenance = {
        "study_id": manifest.get("study_id"),
        "source_commit": manifest.get("source", {}).get("commit"),
        "raw_inputs": {
            str(format_path.relative_to(study_dir)): sha256(format_path) if format_path.exists() else None,
            str(stream_path.relative_to(study_dir)): sha256(stream_path) if stream_path.exists() else None,
        },
        "processor": str(Path(__file__).relative_to(ROOT)),
    }
    processed_dir.mkdir(parents=True, exist_ok=True)
    (processed_dir / "provenance.json").write_text(
        json.dumps(provenance, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
