#!/usr/bin/env python3
"""Generate deterministic telemetry workload datasets for Keydrop research."""

from __future__ import annotations

import argparse
import csv
import math
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
DATASET_DIR = ROOT / "research" / "datasets"


def write_csv(path: Path, rows: list[dict[str, object]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)


def temperature_sensor(count: int) -> list[dict[str, object]]:
    return [
        {
            "timestamp_ms": i * 1000,
            "device_id": f"temp_{i % 8:02d}",
            "temperature_c": round(20.0 + math.sin(i / 12.0) * 4.0, 3),
        }
        for i in range(count)
    ]


def humidity_sensor(count: int) -> list[dict[str, object]]:
    return [
        {
            "timestamp_ms": i * 1000,
            "device_id": f"hum_{i % 8:02d}",
            "humidity_pct": round(55.0 + math.cos(i / 17.0) * 12.0, 3),
        }
        for i in range(count)
    ]


def gps_coordinates(count: int) -> list[dict[str, object]]:
    return [
        {
            "timestamp_ms": i * 200,
            "device_id": "gps_vehicle_01",
            "latitude": round(37.7749 + i * 0.00002, 7),
            "longitude": round(-122.4194 - i * 0.00002, 7),
            "speed_kph": round(35.0 + math.sin(i / 9.0) * 8.0, 2),
        }
        for i in range(count)
    ]


def vehicle_telemetry(count: int) -> list[dict[str, object]]:
    return [
        {
            "timestamp_ms": i * 100,
            "vehicle_id": f"veh_{i % 4:02d}",
            "speed_kph": round(45.0 + math.sin(i / 6.0) * 20.0, 2),
            "rpm": 900 + (i * 37) % 4200,
            "battery_mv": 11800 + (i % 80),
            "fault_code": "" if i % 97 else "P0420",
        }
        for i in range(count)
    ]


def mixed_telemetry(count: int) -> list[dict[str, object]]:
    rows = []
    for i in range(count):
        stream = ("temperature", "humidity", "gps", "vehicle")[i % 4]
        rows.append(
            {
                "timestamp_ms": i * 250,
                "stream": stream,
                "device_id": f"node_{i % 16:02d}",
                "numeric_a": round(10.0 + math.sin(i / 5.0) * 3.0, 3),
                "numeric_b": round(100.0 + math.cos(i / 11.0) * 9.0, 3),
                "text": "stable" if i % 10 else "state-change",
            }
        )
    return rows


def string_heavy(count: int) -> list[dict[str, object]]:
    labels = ["nominal", "warming", "cooling", "maintenance", "calibration"]
    return [
        {
            "timestamp_ms": i * 500,
            "device_id": f"sensor_cluster_alpha_{i % 12:02d}",
            "status": labels[i % len(labels)],
            "location": f"building_a/floor_{i % 5}/rack_{i % 20}",
            "message": "periodic telemetry sample with repeated metadata",
        }
        for i in range(count)
    ]


def constant_stream(count: int) -> list[dict[str, object]]:
    return [
        {
            "timestamp_ms": i * 50,
            "device_id": "constant_01",
            "value": 1000,
            "status": "steady",
        }
        for i in range(count)
    ]


def variable_stream(count: int) -> list[dict[str, object]]:
    return [
        {
            "timestamp_ms": i * 50,
            "device_id": "variable_01",
            "value": 1000 + ((i * 13) % 251),
            "status": "burst" if i % 19 == 0 else "steady",
        }
        for i in range(count)
    ]


def large_payload(count: int) -> list[dict[str, object]]:
    return [
        {
            "timestamp_ms": i * 1000,
            "device_id": f"blob_{i % 4:02d}",
            "payload": "x" * (128 + (i % 8) * 32),
        }
        for i in range(count)
    ]


WORKLOADS = {
    "temperature_sensors.csv": temperature_sensor,
    "humidity_sensors.csv": humidity_sensor,
    "gps_coordinates.csv": gps_coordinates,
    "vehicle_telemetry.csv": vehicle_telemetry,
    "mixed_telemetry.csv": mixed_telemetry,
    "string_heavy_payloads.csv": string_heavy,
    "constant_stream.csv": constant_stream,
    "variable_stream.csv": variable_stream,
    "large_payloads.csv": large_payload,
}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--rows", type=int, default=1000)
    args = parser.parse_args()

    if args.rows <= 0:
        raise SystemExit("--rows must be positive")

    for filename, factory in WORKLOADS.items():
        rows = factory(args.rows)
        write_csv(DATASET_DIR / filename, rows)
        print(f"wrote {DATASET_DIR / filename}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
