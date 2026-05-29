#!/usr/bin/env python3
"""Generate JLCPCB SMT assembly BOM and CPL files from KiCad PCB files."""

from __future__ import annotations

import argparse
import csv
from collections import defaultdict
from pathlib import Path


DEFAULT_BOARDS = [
    ("fork-r0", Path("pcb/fork-r0/fork-r0.kicad_pcb")),
    ("fork-r1", Path("pcb/fork-r1/fork-r1.kicad_pcb")),
]
DEFAULT_OUTPUT = Path("manufacturing/jlcpcb")
LCSC_FIELDS = (
    "LCSC Part #",
    "LCSC",
    "LCSC PN",
    "LCSC Part Number",
    "JLCPCB Part #",
    "JLCPCB Part",
    "JLCPCB Part Number",
)


def tokenize(text: str) -> list[str]:
    tokens: list[str] = []
    i = 0
    while i < len(text):
        char = text[i]
        if char.isspace():
            i += 1
            continue
        if char in "()":
            tokens.append(char)
            i += 1
            continue
        if char == '"':
            i += 1
            value = []
            while i < len(text):
                char = text[i]
                if char == "\\" and i + 1 < len(text):
                    value.append(text[i + 1])
                    i += 2
                    continue
                if char == '"':
                    i += 1
                    break
                value.append(char)
                i += 1
            tokens.append("".join(value))
            continue

        start = i
        while i < len(text) and not text[i].isspace() and text[i] not in "()":
            i += 1
        tokens.append(text[start:i])
    return tokens


def parse_sexpr(tokens: list[str]) -> list:
    root: list = []
    stack = [root]
    for token in tokens:
        if token == "(":
            node: list = []
            stack[-1].append(node)
            stack.append(node)
        elif token == ")":
            if len(stack) == 1:
                raise ValueError("Unexpected closing parenthesis")
            stack.pop()
        else:
            stack[-1].append(token)
    if len(stack) != 1:
        raise ValueError("Unclosed parenthesis in PCB file")
    if len(root) != 1:
        raise ValueError("Expected one top-level KiCad PCB expression")
    return root[0]


def children(node: list, name: str) -> list[list]:
    return [
        item
        for item in node
        if isinstance(item, list) and item and item[0] == name
    ]


def child(node: list, name: str) -> list | None:
    for item in node:
        if isinstance(item, list) and item and item[0] == name:
            return item
    return None


def num(value: object, default: float = 0.0) -> float:
    try:
        return float(value)
    except (TypeError, ValueError):
        return default


def natural_ref_key(reference: str) -> tuple[str, int, str]:
    prefix = "".join(ch for ch in reference if not ch.isdigit())
    digits = "".join(ch for ch in reference if ch.isdigit())
    return (prefix, int(digits or "0"), reference)


def footprint_property(footprint: list, prop_name: str) -> str:
    for prop in children(footprint, "property"):
        if len(prop) >= 3 and prop[1] == prop_name:
            return str(prop[2]).strip()
    return ""


def footprint_properties(footprint: list) -> dict[str, str]:
    props: dict[str, str] = {}
    for prop in children(footprint, "property"):
        if len(prop) >= 3:
            props[str(prop[1])] = str(prop[2]).strip()
    return props


def attr_values(footprint: list) -> set[str]:
    attr = child(footprint, "attr")
    if not attr:
        return set()
    return {str(item) for item in attr[1:] if not isinstance(item, list)}


def is_dnp(footprint: list) -> bool:
    dnp = child(footprint, "dnp")
    if not dnp:
        return False
    return len(dnp) == 1 or str(dnp[1]).lower() in {"yes", "true", "1"}


def is_assembly_component(reference: str, value: str, footprint_name: str) -> bool:
    ref_upper = reference.upper()
    value_upper = value.upper()
    footprint_upper = footprint_name.upper()
    if ref_upper.startswith("#"):
        return False
    if ref_upper.startswith("FID") or "FIDUCIAL" in value_upper or "FIDUCIAL" in footprint_upper:
        return False
    if "LOGO" in ref_upper or "LOGO" in value_upper or "LOGO" in footprint_upper:
        return False
    return True


def first_property(props: dict[str, str], names: tuple[str, ...]) -> str:
    for name in names:
        value = props.get(name, "").strip()
        if value:
            return value
    return ""


def parse_components(board: Path, include_through_hole: bool) -> list[dict]:
    pcb = parse_sexpr(tokenize(board.read_text(encoding="utf-8")))
    components = []

    for footprint in children(pcb, "footprint"):
        props = footprint_properties(footprint)
        attrs = attr_values(footprint)
        reference = props.get("Reference", "")
        value = props.get("Value", "")
        footprint_name = str(footprint[1]) if len(footprint) >= 2 else ""
        at = child(footprint, "at")
        layer = child(footprint, "layer")

        if not reference or not value or not footprint_name or not at or len(at) < 3:
            continue
        if not is_assembly_component(reference, value, footprint_name):
            continue
        if is_dnp(footprint):
            continue
        if "exclude_from_bom" in attrs or "exclude_from_pos_files" in attrs:
            continue
        if "smd" not in attrs and not (include_through_hole and "through_hole" in attrs):
            continue

        layer_name = str(layer[1]) if layer and len(layer) >= 2 else ""
        components.append(
            {
                "reference": reference,
                "comment": value,
                "footprint": footprint_name,
                "lcsc": first_property(props, LCSC_FIELDS),
                "x": num(at[1]),
                "y": -num(at[2]),
                "rotation": num(at[3]) if len(at) >= 4 else 0.0,
                "layer": "Bottom" if layer_name == "B.Cu" else "Top",
            }
        )

    return sorted(components, key=lambda item: natural_ref_key(item["reference"]))


def write_bom(path: Path, components: list[dict]) -> None:
    groups: dict[tuple[str, str, str], list[str]] = defaultdict(list)
    for component in components:
        key = (component["comment"], component["footprint"], component["lcsc"])
        groups[key].append(component["reference"])

    rows = []
    for (comment, footprint, lcsc), references in groups.items():
        refs = sorted(references, key=natural_ref_key)
        rows.append(
            {
                "Comment": comment,
                "Designator": ",".join(refs),
                "Footprint": footprint,
                "LCSC Part #": lcsc,
            }
        )
    rows.sort(key=lambda item: natural_ref_key(item["Designator"].split(",")[0]))

    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(
            handle,
            fieldnames=["Comment", "Designator", "Footprint", "LCSC Part #"],
        )
        writer.writeheader()
        writer.writerows(rows)


def write_cpl(path: Path, components: list[dict]) -> None:
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(
            handle,
            fieldnames=["Designator", "Mid X", "Mid Y", "Layer", "Rotation"],
        )
        writer.writeheader()
        for component in components:
            writer.writerow(
                {
                    "Designator": component["reference"],
                    "Mid X": f"{component['x']:.4f}",
                    "Mid Y": f"{component['y']:.4f}",
                    "Layer": component["layer"],
                    "Rotation": f"{component['rotation']:.4f}",
                }
            )


def write_missing_lcsc(path: Path, components: list[dict]) -> None:
    missing = [component for component in components if not component["lcsc"]]
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(
            handle,
            fieldnames=["Designator", "Comment", "Footprint", "Layer"],
        )
        writer.writeheader()
        for component in missing:
            writer.writerow(
                {
                    "Designator": component["reference"],
                    "Comment": component["comment"],
                    "Footprint": component["footprint"],
                    "Layer": component["layer"],
                }
            )


def generate(name: str, board: Path, output_root: Path, include_through_hole: bool) -> None:
    output = output_root / name
    output.mkdir(parents=True, exist_ok=True)
    components = parse_components(board, include_through_hole)

    write_bom(output / "bom.csv", components)
    write_cpl(output / "cpl.csv", components)
    write_missing_lcsc(output / "missing_lcsc.csv", components)

    lcsc_count = sum(1 for component in components if component["lcsc"])
    print(f"{name}: wrote {output / 'bom.csv'}")
    print(f"{name}: wrote {output / 'cpl.csv'}")
    print(f"{name}: components={len(components)} with_lcsc={lcsc_count} missing_lcsc={len(components) - lcsc_count}")


def parse_board_arg(value: str) -> tuple[str, Path]:
    if ":" not in value:
        path = Path(value)
        return (path.stem, path)
    name, path = value.split(":", 1)
    return (name, Path(path))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--board",
        action="append",
        type=parse_board_arg,
        help="Board to export, as name:path or just path. Can be repeated.",
    )
    parser.add_argument("--out", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument(
        "--include-through-hole",
        action="store_true",
        help="Include through-hole footprints in addition to SMD footprints.",
    )
    args = parser.parse_args()

    boards = args.board if args.board else DEFAULT_BOARDS
    for name, board in boards:
        generate(name, board, args.out, args.include_through_hole)


if __name__ == "__main__":
    main()
