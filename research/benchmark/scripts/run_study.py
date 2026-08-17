#!/usr/bin/env python3
"""Create one immutable, reproducible Keydrop benchmark study.

Each invocation writes all inputs and outputs beneath
``research/benchmark/studies/<study-id>``.  The script intentionally refuses
to reuse a study id: benchmark evidence must be append-only.
"""

from __future__ import annotations

import argparse
import json
import os
import platform
import shlex
import shutil
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
STUDIES_DIR = ROOT / "research" / "benchmark" / "studies"


def run(
    command: list[str], *, cwd: Path, log_path: Path, environment: dict[str, str], label: str
) -> subprocess.CompletedProcess[str]:
    """Run a command, preserving its exact output and invocation."""
    print(f"[{label}] running: {shlex.join(command)}", flush=True)
    result = subprocess.run(
        command, cwd=cwd, env=environment, text=True, capture_output=True, check=False
    )
    log_path.parent.mkdir(parents=True, exist_ok=True)
    log_path.write_text(
        "$ " + shlex.join(command) + "\n\n[stdout]\n" + result.stdout
        + "\n[stderr]\n" + result.stderr
        + f"\n[exit_code]\n{result.returncode}\n",
        encoding="utf-8",
    )
    print(f"[{label}] exit code {result.returncode}; log: {log_path}", flush=True)
    return result


def git_value(*args: str) -> str:
    result = subprocess.run(["git", *args], cwd=ROOT, text=True, capture_output=True, check=False)
    return result.stdout.strip() if result.returncode == 0 else "unavailable"


def command_version(command: list[str]) -> str:
    result = subprocess.run(command, cwd=ROOT, text=True, capture_output=True, check=False)
    output = (result.stdout or result.stderr).strip().splitlines()
    return output[0] if result.returncode == 0 and output else "unavailable"


def cpu_model() -> str:
    cpuinfo = Path("/proc/cpuinfo")
    if cpuinfo.exists():
        for line in cpuinfo.read_text(encoding="utf-8", errors="replace").splitlines():
            if line.startswith("model name"):
                return line.split(":", 1)[1].strip()
    return platform.processor() or "unavailable"


def memory_bytes() -> int | None:
    meminfo = Path("/proc/meminfo")
    if meminfo.exists():
        for line in meminfo.read_text(encoding="utf-8", errors="replace").splitlines():
            if line.startswith("MemTotal:"):
                return int(line.split()[1]) * 1024
    return None


def write_manifest(path: Path, manifest: dict[str, object]) -> None:
    path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--study-id", help="new immutable study identifier")
    parser.add_argument("--build-dir", default="build/research")
    parser.add_argument("--iterations", type=int, default=100_000)
    parser.add_argument("--trials", type=int, default=30)
    parser.add_argument("--skip-stream", action="store_true")
    parser.add_argument("--generator", help="optional CMake generator")
    parser.add_argument(
        "--allow-dirty",
        action="store_true",
        help="permit a development-only study from a dirty worktree",
    )
    args = parser.parse_args()

    if args.iterations <= 0 or args.trials < 30:
        parser.error("--iterations must be positive and --trials must be at least 30")

    timestamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    commit = git_value("rev-parse", "HEAD")
    dirty_paths = git_value("status", "--porcelain").splitlines()
    if dirty_paths and not args.allow_dirty:
        parser.error(
            "refusing to create a trusted study from a dirty worktree; commit or stash changes, "
            "or use --allow-dirty for a development-only validation run"
        )
    study_id = args.study_id or f"phase0-{timestamp}-{commit[:12]}"
    study_dir = STUDIES_DIR / study_id
    if study_dir.exists():
        parser.error(f"study already exists and is immutable: {study_dir}")

    build_dir = (ROOT / args.build_dir).resolve()
    ccache_dir = build_dir / ".ccache"
    ccache_dir.mkdir(parents=True, exist_ok=True)
    command_environment = os.environ.copy()
    command_environment.setdefault("CCACHE_DIR", str(ccache_dir))
    command = [sys.executable, str(Path(__file__).with_name("run_benchmarks.py")),
               "--build-dir", str(build_dir), "--iterations", str(args.iterations),
               "--trials", str(args.trials), "--output-dir", str(study_dir / "raw")]
    if args.skip_stream:
        command.append("--skip-stream")
    configure = ["cmake", "-S", str(ROOT), "-B", str(build_dir), "-DCMAKE_BUILD_TYPE=Release"]
    if args.generator:
        configure.extend(["-G", args.generator])
    build = ["cmake", "--build", str(build_dir), "--parallel"]
    test = ["ctest", "--test-dir", str(build_dir), "--output-on-failure", "--timeout", "60"]

    study_dir.mkdir(parents=True)
    manifest_path = study_dir / "manifest.json"
    manifest: dict[str, object] = {
        "study_format": 1,
        "status": "created",
        "study_kind": "development_validation" if dirty_paths else "trusted_baseline",
        "publication_eligible": not dirty_paths,
        "created_at_utc": timestamp,
        "study_id": study_id,
        "source": {
            "commit": commit,
            "branch": git_value("branch", "--show-current"),
            "dirty": bool(dirty_paths),
            "dirty_paths": dirty_paths,
        },
        "environment": {
            "os": platform.platform(),
            "architecture": platform.machine(),
            "cpu_model": cpu_model(),
            "logical_cpu_count": os.cpu_count(),
            "memory_bytes": memory_bytes(),
            "python": sys.version.split()[0],
            "cmake": command_version(["cmake", "--version"]),
            "compiler": command_version(["c++", "--version"]),
            "ccache_dir": command_environment.get("CCACHE_DIR", "not set"),
        },
        "configuration": {
            "build_dir": str(build_dir),
            "build_type": "Release",
            "cmake_generator": args.generator or "default",
            "iterations": args.iterations,
            "trials": args.trials,
            "stream_benchmark": not args.skip_stream,
            "benchmark_classification": "in-repository development baselines",
        },
        "commands": {
            "configure": configure,
            "build": build,
            "test": test,
            "benchmark": command,
            "process": [sys.executable, "research/benchmark/scripts/process_results.py", "--study", str(study_dir)],
            "plot": [sys.executable, "research/benchmark/scripts/plot_graphs.py", "--study", str(study_dir)],
        },
        "outputs": {
            "test_log": "test/ctest.log",
            "raw_dir": "raw",
            "processed_dir": "processed",
            "graphs_dir": "graphs",
        },
    }
    write_manifest(manifest_path, manifest)

    configure_result = run(
        configure,
        cwd=ROOT,
        log_path=study_dir / "build" / "configure.log",
        environment=command_environment,
        label="configure",
    )
    if configure_result.returncode != 0:
        manifest["status"] = "configure_failed"
        write_manifest(manifest_path, manifest)
        return configure_result.returncode

    build_result = run(
        build,
        cwd=ROOT,
        log_path=study_dir / "build" / "build.log",
        environment=command_environment,
        label="build",
    )
    if build_result.returncode != 0:
        manifest["status"] = "build_failed"
        write_manifest(manifest_path, manifest)
        return build_result.returncode

    test_result = run(
        test,
        cwd=ROOT,
        log_path=study_dir / "test" / "ctest.log",
        environment=command_environment,
        label="test",
    )
    if test_result.returncode != 0:
        manifest["status"] = "test_failed"
        write_manifest(manifest_path, manifest)
        return test_result.returncode

    benchmark_result = run(
        command,
        cwd=ROOT,
        log_path=study_dir / "raw" / "runner.log",
        environment=command_environment,
        label="benchmark",
    )
    if benchmark_result.returncode != 0:
        manifest["status"] = "benchmark_failed"
        write_manifest(manifest_path, manifest)
        return benchmark_result.returncode

    process_result = run(
        manifest["commands"]["process"],  # type: ignore[arg-type,index]
        cwd=ROOT,
        log_path=study_dir / "processed" / "process.log",
        environment=command_environment,
        label="process",
    )
    if process_result.returncode != 0:
        manifest["status"] = "processing_failed"
        write_manifest(manifest_path, manifest)
        return process_result.returncode

    plot_result = run(
        manifest["commands"]["plot"],  # type: ignore[arg-type,index]
        cwd=ROOT,
        log_path=study_dir / "graphs" / "plot.log",
        environment=command_environment,
        label="plot",
    )
    manifest["status"] = "complete" if plot_result.returncode == 0 else "plot_failed"
    write_manifest(manifest_path, manifest)
    return plot_result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
