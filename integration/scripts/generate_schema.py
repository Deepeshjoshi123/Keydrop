#!/usr/bin/env python3
"""Generate a Keydrop telemetry schema JSON from CLI arguments.

Usage:
  python3 generate_schema.py --name MySchema --id 100 \
      --field temperature:u16 --field humidity:u16 --field device_id:string:64

Output:  JSON on stdout (redirect to a file)
"""

import argparse
import json
import sys


def main():
    parser = argparse.ArgumentParser(description="Generate a Keydrop schema JSON")
    parser.add_argument("--name", required=True, help="Schema name")
    parser.add_argument("--id", type=int, required=True, help="Message ID (1-65535)")
    parser.add_argument("--field", action="append", default=[],
                        help="Field spec: name:type[:max_length]  e.g. temperature:u16  device_id:string:64")
    args = parser.parse_args()

    if not 1 <= args.id <= 65535:
        print("Error: message_id must be 1-65535", file=sys.stderr)
        sys.exit(1)

    fields = []
    for idx, spec in enumerate(args.field):
        parts = spec.split(":")
        if len(parts) < 2:
            print(f"Error: invalid field spec '{spec}' — use name:type[:max_len]", file=sys.stderr)
            sys.exit(1)
        name, ftype = parts[0], parts[1]
        max_len = int(parts[2]) if len(parts) > 2 else None

        field = {"name": name, "type": ftype, "index": idx}
        if max_len is not None:
            field["max_length"] = max_len
        fields.append(field)

    schema = {
        "name": args.name,
        "message_id": args.id,
        "fields": fields,
    }

    json.dump(schema, sys.stdout, indent=2)
    print()


if __name__ == "__main__":
    main()
