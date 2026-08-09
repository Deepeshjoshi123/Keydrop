#!/usr/bin/env python3
"""Generate architecture diagrams as SVG and optional PNG files."""

from __future__ import annotations

from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
OUT_DIR = ROOT / "research" / "diagrams"


def box(x: int, y: int, w: int, h: int, label: str, fill: str = "#F7F9FB") -> str:
    return (
        f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="6" fill="{fill}" stroke="#1F2933" stroke-width="1.5"/>'
        f'<text x="{x + w / 2}" y="{y + h / 2 + 5}" text-anchor="middle" font-family="Arial" font-size="14">{label}</text>'
    )


def arrow(x1: int, y1: int, x2: int, y2: int) -> str:
    return (
        f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="#1F2933" stroke-width="1.5" marker-end="url(#arrow)"/>'
    )


def svg(title: str, body: list[str], width: int = 1100, height: int = 650) -> str:
    return "\n".join(
        [
            f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
            "<defs><marker id=\"arrow\" markerWidth=\"10\" markerHeight=\"10\" refX=\"8\" refY=\"3\" orient=\"auto\" markerUnits=\"strokeWidth\"><path d=\"M0,0 L0,6 L9,3 z\" fill=\"#1F2933\"/></marker></defs>",
            '<rect width="100%" height="100%" fill="white"/>',
            f'<text x="{width / 2}" y="34" text-anchor="middle" font-family="Arial" font-size="22" font-weight="700">{title}</text>',
            *body,
            "</svg>",
        ]
    )


def write(name: str, title: str, body: list[str]) -> None:
    path = OUT_DIR / f"{name}.svg"
    path.write_text(svg(title, body), encoding="utf-8")
    print(f"wrote {path}")


def main() -> int:
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    write(
        "overall_architecture",
        "Overall Architecture",
        [
            box(70, 110, 180, 70, "Application"),
            box(330, 90, 210, 70, "SchemaRuntime", "#EEF6FF"),
            box(650, 90, 170, 70, "Reliability"),
            box(870, 90, 170, 70, "Transport"),
            box(330, 220, 210, 70, "Schema Registry"),
            box(70, 340, 180, 70, "Core Buffer"),
            box(330, 340, 210, 70, "Encoder / Reader"),
            box(650, 340, 170, 70, "Optimizers"),
            box(870, 340, 170, 70, "TCP / WS"),
            arrow(250, 145, 330, 125),
            arrow(540, 125, 650, 125),
            arrow(820, 125, 870, 125),
            arrow(435, 160, 435, 220),
            arrow(435, 290, 435, 340),
            arrow(250, 375, 330, 375),
            arrow(540, 375, 650, 375),
            arrow(820, 375, 870, 375),
        ],
    )

    write(
        "encoding_pipeline",
        "Encoding Pipeline",
        [
            box(70, 140, 170, 65, "NamedPayload"),
            box(290, 140, 170, 65, "FieldMapper"),
            box(510, 140, 170, 65, "Encoder"),
            box(730, 140, 170, 65, "Optimizer"),
            box(930, 140, 120, 65, "Buffer"),
            arrow(240, 172, 290, 172),
            arrow(460, 172, 510, 172),
            arrow(680, 172, 730, 172),
            arrow(900, 172, 930, 172),
            box(290, 280, 170, 65, "SchemaDef"),
            box(510, 280, 170, 65, "PacketLayout"),
            arrow(375, 280, 375, 205),
            arrow(595, 280, 595, 205),
        ],
    )

    write(
        "decoding_pipeline",
        "Decoding Pipeline",
        [
            box(70, 140, 150, 65, "Buffer"),
            box(270, 140, 170, 65, "Deoptimizer"),
            box(490, 140, 180, 65, "Corruption Check"),
            box(720, 140, 160, 65, "PacketReader"),
            box(930, 140, 140, 65, "NamedPayload"),
            arrow(220, 172, 270, 172),
            arrow(440, 172, 490, 172),
            arrow(670, 172, 720, 172),
            arrow(880, 172, 930, 172),
            box(490, 280, 180, 65, "Schema Registry"),
            arrow(580, 280, 580, 205),
        ],
    )

    write(
        "packet_format",
        "Packet Format",
        [
            box(120, 180, 160, 80, "message_id u16", "#EAF7EA"),
            box(280, 180, 190, 80, "field 0 bytes"),
            box(470, 180, 190, 80, "field 1 bytes"),
            box(660, 180, 220, 80, "variable length fields"),
            box(120, 350, 160, 80, "message_id u16", "#EAF7EA"),
            box(280, 350, 100, 80, "0xFD"),
            box(380, 350, 130, 80, "bitmap size"),
            box(510, 350, 160, 80, "zero bitmap"),
            box(670, 350, 210, 80, "remaining body"),
            '<text x="120" y="155" font-family="Arial" font-size="15" font-weight="700">Normal packet</text>',
            '<text x="120" y="325" font-family="Arial" font-size="15" font-weight="700">Optimized packet</text>',
        ],
    )

    write(
        "transport_layer",
        "Transport Layer",
        [
            box(80, 140, 180, 65, "Transport API"),
            box(330, 90, 190, 65, "TcpAdapter"),
            box(330, 220, 190, 65, "WebSocketAdapter"),
            box(610, 140, 190, 65, "Native Socket"),
            box(860, 140, 170, 65, "Network"),
            arrow(260, 172, 330, 122),
            arrow(260, 172, 330, 252),
            arrow(520, 122, 610, 172),
            arrow(520, 252, 610, 172),
            arrow(800, 172, 860, 172),
        ],
    )

    write(
        "reliability_layer",
        "Reliability Layer",
        [
            box(80, 150, 170, 65, "Packet Stream"),
            box(320, 150, 190, 65, "PacketSynchronizer"),
            box(580, 150, 190, 65, "CorruptionDetector"),
            box(840, 150, 170, 65, "Decoded Packet"),
            arrow(250, 182, 320, 182),
            arrow(510, 182, 580, 182),
            arrow(770, 182, 840, 182),
            box(320, 300, 190, 65, "SchemaRegistry"),
            box(580, 300, 190, 65, "PacketLayout"),
            arrow(415, 300, 415, 215),
            arrow(675, 300, 675, 215),
        ],
    )

    write(
        "memory_management",
        "Memory Management",
        [
            box(80, 150, 170, 65, "Buffer"),
            box(310, 150, 170, 65, "BufferView"),
            box(540, 150, 170, 65, "BufferPool"),
            box(770, 150, 170, 65, "BufferLease"),
            box(310, 300, 170, 65, "PayloadPool"),
            box(540, 300, 170, 65, "PayloadLease"),
            arrow(250, 182, 310, 182),
            arrow(710, 182, 770, 182),
            arrow(480, 332, 540, 332),
        ],
    )

    write(
        "runtime_flow",
        "Runtime Flow",
        [
            box(80, 120, 170, 65, "register_schema"),
            box(310, 120, 170, 65, "send"),
            box(540, 120, 170, 65, "send_stream"),
            box(770, 120, 170, 65, "Transport send"),
            box(310, 280, 170, 65, "receive"),
            box(540, 280, 170, 65, "receive_stream"),
            box(770, 280, 170, 65, "Recovered stream"),
            arrow(250, 152, 310, 152),
            arrow(480, 152, 540, 152),
            arrow(710, 152, 770, 152),
            arrow(770, 280, 710, 312),
            arrow(540, 312, 480, 312),
        ],
    )

    write(
        "schema_runtime",
        "Schema Runtime",
        [
            box(70, 120, 180, 65, "SchemaRuntime", "#EEF6FF"),
            box(320, 70, 180, 65, "SchemaRegistry"),
            box(320, 170, 180, 65, "RuntimeOptimizer"),
            box(320, 270, 180, 65, "AdaptiveDictionary"),
            box(580, 70, 180, 65, "StreamOptimizer"),
            box(580, 170, 180, 65, "BufferPool"),
            box(580, 270, 180, 65, "PayloadPool"),
            arrow(250, 152, 320, 102),
            arrow(250, 152, 320, 202),
            arrow(250, 152, 320, 302),
            arrow(500, 102, 580, 102),
            arrow(500, 202, 580, 202),
            arrow(500, 302, 580, 302),
        ],
    )

    write(
        "packet_lifecycle",
        "Packet Lifecycle",
        [
            box(70, 130, 150, 65, "SchemaDef"),
            box(270, 130, 150, 65, "Payload"),
            box(470, 130, 150, 65, "Packet"),
            box(670, 130, 150, 65, "Stream"),
            box(870, 130, 150, 65, "Transport"),
            box(470, 300, 150, 65, "Decode"),
            box(270, 300, 150, 65, "Validate"),
            arrow(220, 162, 270, 162),
            arrow(420, 162, 470, 162),
            arrow(620, 162, 670, 162),
            arrow(820, 162, 870, 162),
            arrow(545, 195, 545, 300),
            arrow(470, 332, 420, 332),
        ],
    )

    write(
        "configuration_flow",
        "Configuration Flow",
        [
            box(90, 120, 190, 65, "Runtime Config"),
            box(350, 70, 210, 65, "Optimizer Config"),
            box(350, 170, 210, 65, "Dictionary Config"),
            box(350, 270, 210, 65, "Stream Config"),
            box(650, 120, 210, 65, "Transport Config"),
            box(650, 270, 210, 65, "Pool Config"),
            arrow(280, 152, 350, 102),
            arrow(280, 152, 350, 202),
            arrow(280, 152, 350, 302),
            arrow(560, 102, 650, 152),
            arrow(560, 302, 650, 302),
        ],
    )

    write(
        "cross_platform_networking",
        "Cross Platform Networking",
        [
            box(80, 130, 190, 65, "TcpAdapter"),
            box(340, 70, 200, 65, "Windows Winsock"),
            box(340, 210, 200, 65, "POSIX sockets"),
            box(640, 130, 210, 65, "platform::socket"),
            box(900, 130, 150, 65, "Network"),
            arrow(270, 162, 340, 102),
            arrow(270, 162, 340, 242),
            arrow(540, 102, 640, 162),
            arrow(540, 242, 640, 162),
            arrow(850, 162, 900, 162),
        ],
    )

    write(
        "buffer_layout",
        "Buffer Layout",
        [
            box(100, 140, 220, 85, "std::vector<byte>"),
            box(320, 140, 170, 85, "size"),
            box(490, 140, 170, 85, "capacity"),
            box(660, 140, 220, 85, "mutable bytes"),
            box(100, 330, 220, 85, "BufferView"),
            box(320, 330, 170, 85, "data pointer"),
            box(490, 330, 170, 85, "view size"),
            box(660, 330, 220, 85, "slice"),
            arrow(320, 182, 320, 372),
            arrow(880, 372, 880, 225),
        ],
    )

    write(
        "component_relationships",
        "Component Relationships",
        [
            box(80, 90, 170, 65, "core"),
            box(350, 90, 170, 65, "schema"),
            box(620, 90, 170, 65, "reliability"),
            box(890, 90, 170, 65, "transport"),
            box(350, 250, 170, 65, "benchmark"),
            box(620, 250, 170, 65, "platform"),
            arrow(250, 122, 350, 122),
            arrow(520, 122, 620, 122),
            arrow(790, 122, 890, 122),
            arrow(435, 250, 435, 155),
            arrow(705, 250, 890, 155),
        ],
    )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
