#!/usr/bin/env python3
"""Run Keydrop benchmark executables and capture raw CSV."""

from __future__ import annotations

import argparse
import csv
import platform
import re
import subprocess
from datetime import datetime, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
RAW_DIR = ROOT / "research" / "benchmark" / "raw_data"


def exe_path(build_dir: Path, name: str) -> Path:
    candidates = [
        build_dir / "bin" / f"{name}.exe",
        build_dir / "bin" / name,
        build_dir / "bin" / "Release" / f"{name}.exe",
        build_dir / "bin" / "Release" / name,
        build_dir / "Release" / f"{name}.exe",
        build_dir / name,
        build_dir / f"{name}.exe",
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate
    raise FileNotFoundError(f"could not find {name} under {build_dir}")


def parse_table(text: str) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    pattern = re.compile(
        r"^(?P<format>\w+)\s+"
        r"(?P<packet_size_bytes>\d+)\s+"
        r"(?P<encode_latency_ns>[0-9.]+)\s+"
        r"(?P<decode_latency_ns>[0-9.]+)\s+"
        r"(?P<allocations>\d+)\s+"
        r"(?P<allocated_bytes>\d+)\s+"
        r"(?P<throughput_per_sec>[0-9.]+)"
    )
    for line in text.splitlines():
        match = pattern.match(line.strip())
        if match:
            rows.append(match.groupdict())
    return rows


def parse_key_values(text: str) -> dict[str, str]:
    values: dict[str, str] = {}
    for line in text.splitlines():
        if "=" in line:
            key, value = line.split("=", 1)
            values[key.strip()] = value.strip()
    return values


def write_rows(path: Path, rows: list[dict[str, object]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", default="build/research")
    parser.add_argument("--iterations", type=int, default=1000)
    parser.add_argument("--trials", type=int, default=30)
    parser.add_argument("--skip-stream", action="store_true")
    args = parser.parse_args()

    build_dir = (ROOT / args.build_dir).resolve()
    benchmark_runner = exe_path(build_dir, "benchmark_runner")
    stream_runner = exe_path(build_dir, "stream_optimizer_benchmark")

    timestamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    metadata = {
        "timestamp_utc": timestamp,
        "system": platform.system(),
        "machine": platform.machine(),
        "processor": platform.processor(),
        "python": platform.python_version(),
        "iterations": args.iterations,
    }

    format_rows: list[dict[str, object]] = []
    stream_rows: list[dict[str, object]] = []

    for trial in range(args.trials):
        report_path = RAW_DIR / f"format_report_trial_{trial}.txt"
        result = subprocess.run(
            [str(benchmark_runner), str(args.iterations), str(report_path)],
            cwd=ROOT,
            text=True,
            capture_output=True,
            check=True,
        )
        for row in parse_table(result.stdout):
            format_rows.append({"trial": trial, **metadata, **row})

        if not args.skip_stream:
            stream = subprocess.run(
                [str(stream_runner)],
                cwd=ROOT,
                text=True,
                capture_output=True,
                check=True,
            )
            stream_rows.append({"trial": trial, **metadata, **parse_key_values(stream.stdout)})

    write_rows(RAW_DIR / f"format_benchmark_{timestamp}.csv", format_rows)
    if stream_rows:
        write_rows(RAW_DIR / f"stream_optimizer_{timestamp}.csv", stream_rows)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
