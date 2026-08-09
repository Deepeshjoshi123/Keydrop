#!/usr/bin/env python3
"""Generate the IEEE Table I comparison figure from comparison_table.csv."""

from __future__ import annotations

import csv
import os
import textwrap
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TABLE_DIR = ROOT / "research" / "tables"
CSV_PATH = TABLE_DIR / "comparison_table.csv"
PNG_PATH = TABLE_DIR / "comparison_table.png"
SVG_PATH = TABLE_DIR / "comparison_table.svg"

MPL_CONFIG_DIR = ROOT / "build" / "matplotlib-cache"
MPL_CONFIG_DIR.mkdir(parents=True, exist_ok=True)
os.environ.setdefault("MPLCONFIGDIR", str(MPL_CONFIG_DIR))

import matplotlib.pyplot as plt


def wrap_cell(value: str, width: int) -> str:
    if len(value) <= width:
        return value
    return "\n".join(textwrap.wrap(value, width=width, break_long_words=False))


def load_table() -> tuple[list[str], list[list[str]]]:
    with CSV_PATH.open("r", newline="", encoding="utf-8") as handle:
        reader = csv.reader(handle)
        rows = list(reader)
    if not rows:
        raise ValueError(f"{CSV_PATH} is empty")
    return rows[0], rows[1:]


def build_figure(headers: list[str], rows: list[list[str]]) -> plt.Figure:
    wrap_widths = [16, 10, 17, 10, 9, 18, 13, 13, 11, 11, 12, 16]
    wrapped_headers = [wrap_cell(header, width) for header, width in zip(headers, wrap_widths)]
    wrapped_rows = [
        [wrap_cell(cell, width) for cell, width in zip(row, wrap_widths)]
        for row in rows
    ]

    plt.rcParams.update(
        {
            "font.family": "serif",
            "font.serif": ["Times New Roman", "Times", "DejaVu Serif"],
            "svg.fonttype": "none",
            "figure.facecolor": "white",
            "savefig.facecolor": "white",
        }
    )

    fig, ax = plt.subplots(figsize=(18.0, 5.0), dpi=300)
    ax.axis("off")

    column_widths = [
        0.105,
        0.075,
        0.135,
        0.075,
        0.065,
        0.135,
        0.105,
        0.095,
        0.085,
        0.080,
        0.090,
        0.130,
    ]
    total = sum(column_widths)
    column_widths = [width / total for width in column_widths]

    table = ax.table(
        cellText=wrapped_rows,
        colLabels=wrapped_headers,
        cellLoc="center",
        colLoc="center",
        colWidths=column_widths,
        loc="center",
        bbox=[0.005, 0.02, 0.99, 0.86],
    )

    table.auto_set_font_size(False)
    table.set_fontsize(7.3)

    for (row_idx, _col_idx), cell in table.get_celld().items():
        cell.set_edgecolor("black")
        cell.set_linewidth(0.45)
        cell.PAD = 0.035
        if row_idx == 0:
            cell.set_facecolor("#f0f0f0")
            cell.set_text_props(weight="bold", color="black", ha="center", va="center")
            cell.set_height(0.125)
        else:
            cell.set_facecolor("#ffffff" if row_idx % 2 else "#f7f7f7")
            cell.set_text_props(color="black", ha="center", va="center")
            cell.set_height(0.104)

    ax.text(
        0.5,
        0.955,
        "TABLE I",
        ha="center",
        va="center",
        fontsize=8.5,
        fontweight="bold",
        transform=ax.transAxes,
    )
    ax.text(
        0.5,
        0.915,
        "Comparison of Existing Telemetry Serialization Systems",
        ha="center",
        va="center",
        fontsize=8.0,
        transform=ax.transAxes,
    )

    return fig


def main() -> None:
    headers, rows = load_table()
    fig = build_figure(headers, rows)
    fig.savefig(PNG_PATH, dpi=300, bbox_inches="tight", pad_inches=0.04)
    fig.savefig(SVG_PATH, bbox_inches="tight", pad_inches=0.04)
    plt.close(fig)


if __name__ == "__main__":
    main()
