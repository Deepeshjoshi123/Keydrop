#!/usr/bin/env python3
"""Generate random sensor telemetry data as CSV or JSON for testing.

Usage:
  python3 generate_telemetry_data.py --format csv --rows 100
  python3 generate_telemetry_data.py --format json --rows 50

Output on stdout — redirect to a file.
"""

import argparse
import json
import random
import sys


SENSOR_IDS = ["sensor-01", "sensor-02", "sensor-03", "sensor-07", "sensor-12"]


def main():
    parser = argparse.ArgumentParser(description="Generate test telemetry data")
    parser.add_argument("--format", choices=["csv", "json"], default="csv")
    parser.add_argument("--rows", type=int, default=100)
    args = parser.parse_args()

    if args.format == "csv":
        print("temperature,humidity,device_id")
        for _ in range(args.rows):
            temp = round(random.uniform(15.0, 45.0), 1)
            hum = round(random.uniform(20.0, 95.0), 1)
            dev = random.choice(SENSOR_IDS)
            print(f"{temp},{hum},{dev}")

    elif args.format == "json":
        records = []
        for _ in range(args.rows):
            records.append({
                "temperature": round(random.uniform(15.0, 45.0), 1),
                "humidity":    round(random.uniform(20.0, 95.0), 1),
                "device_id":   random.choice(SENSOR_IDS),
            })
        json.dump(records, sys.stdout, indent=2)
        print()


if __name__ == "__main__":
    main()
