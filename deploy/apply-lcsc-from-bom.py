#!/usr/bin/env python3
"""Populate KiCad LCSC fields from a trusted BOM when value and footprint match."""

from __future__ import annotations

import argparse
import csv
import json
import re
import uuid
from dataclasses import dataclass
from pathlib import Path


BOARDS = {
    "fork-r0": {
        "bom": Path("manufacturing/jlcpcb/fork-r0/bom.csv"),
        "pcb": Path("pcb/fork-r0/fork-r0.kicad_pcb"),
        "sch": Path("pcb/fork-r0/fork-r0.kicad_sch"),
    },
    "fork-r1": {
        "bom": Path("manufacturing/jlcpcb/fork-r1/bom.csv"),
        "pcb": Path("pcb/fork-r1/fork-r1.kicad_pcb"),
        "sch": Path("pcb/fork-r1/fork-r1.kicad_sch"),
    },
}

FOOTPRINT_PACKAGES = (
    (re.compile(r"0603|1608", re.I), "0603"),
    (re.compile(r"0805|2012", re.I), "0805"),
    (re.compile(r"TSSOP-14", re.I), "TSSOP-14"),
    (re.compile(r"TSSOP-16", re.I), "TSSOP-16"),
    (re.compile(r"TSSOP-24", re.I), "TSSOP-24"),
    (re.compile(r"SOIC-8", re.I), "SOIC-8"),
    (re.compile(r"SOT-23-5", re.I), "SOT-23-5"),
    (re.compile(r"QFN-48", re.I), "QFN-48"),
    (re.compile(r"5032|5\.0x3\.2|SG8002", re.I), "SMD5032-4P"),
    (re.compile(r"SRN4018|4018|L_Bourns-SRN4018", re.I), "4018"),
    (re.compile(r"R_Array_Convex_4x0603|RA4_1206|RES_ARRAY41206", re.I), "0603x4"),
)


@dataclass(frozen=True)
class SourcePart:
    lcsc: str
    value: str
    footprint: str
    source_refs: str


@dataclass(frozen=True)
class Assignment:
    board: str
    reference: str
    value: str
    footprint: str
    lcsc: str
    source_refs: str
    status: str
    reason: str


def read_csv(path: Path) -> list[dict[str, str]]:
    with path.open(newline="", encoding="utf-8-sig") as handle:
        return list(csv.DictReader(handle))


def write_csv(path: Path, rows: list[dict[str, str]], fieldnames: list[str]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def split_refs(text: str) -> list[str]:
    return [ref.strip() for ref in text.split(",") if ref.strip()]


def ref_prefix(refs: str) -> str:
    first = split_refs(refs)[0] if split_refs(refs) else refs
    match = re.match(r"[A-Za-z]+", first)
    return match.group(0).upper() if match else ""


def normalize_value(value: str, prefix: str) -> str:
    text = value.strip().replace("µ", "u").lower()
    text = re.sub(r"\s+", "", text)
    if prefix == "R":
        text = text.replace("ohm", "r").replace("Ω", "r")
    return text


def package_from_footprint(footprint: str) -> str:
    for pattern, package in FOOTPRINT_PACKAGES:
        if pattern.search(footprint):
            return package
    return footprint.strip().lower()


def key_for(row: dict[str, str], designator_field: str, value_field: str, footprint_field: str) -> tuple[str, str, str]:
    prefix = ref_prefix(row.get(designator_field, ""))
    return (
        prefix,
        normalize_value(row.get(value_field, ""), prefix),
        package_from_footprint(row.get(footprint_field, "")),
    )


def load_source_parts(path: Path) -> tuple[dict[tuple[str, str, str], SourcePart], list[dict[str, str]]]:
    by_key: dict[tuple[str, str, str], SourcePart] = {}
    skipped: list[dict[str, str]] = []

    for row in read_csv(path):
        lcsc = row.get("LCSC Part #", "").strip()
        if not lcsc:
            continue

        key = key_for(row, "Designator", "Value", "Footprint")
        source = SourcePart(
            lcsc=lcsc,
            value=row.get("Value", ""),
            footprint=row.get("Footprint", ""),
            source_refs=row.get("Designator", ""),
        )

        existing = by_key.get(key)
        if existing and existing.lcsc != lcsc:
            skipped.append({
                "Status": "skipped",
                "Reason": "ambiguous-source-key",
                "Key": " / ".join(key),
                "Existing LCSC": existing.lcsc,
                "New LCSC": lcsc,
                "Source Designators": row.get("Designator", ""),
            })
            continue
        by_key[key] = source

    return by_key, skipped


def load_availability(path: Path | None) -> dict[str, dict]:
    if not path:
        return {}
    data = json.loads(path.read_text(encoding="utf-8"))
    return data.get("parts", {})


def availability_status(lcsc: str, availability: dict[str, dict]) -> tuple[str, str]:
    if not availability:
        return "unknown", "availability-not-checked"
    result = availability.get(lcsc)
    if not result:
        return "skipped", "not-present-in-availability-file"
    if result.get("available"):
        return "available", f"stock={result.get('stock', '')}"
    if result.get("found"):
        return "skipped", f"found-but-no-stock stock={result.get('stock', '')}"
    return "skipped", result.get("error", "not-found")


def collect_assignments(source: dict[tuple[str, str, str], SourcePart], availability: dict[str, dict]) -> list[Assignment]:
    assignments: list[Assignment] = []

    for board, paths in BOARDS.items():
        for row in read_csv(paths["bom"]):
            key = key_for(row, "Designator", "Comment", "Footprint")
            source_part = source.get(key)
            if not source_part:
                continue

            status, reason = availability_status(source_part.lcsc, availability)
            if status != "available":
                for reference in split_refs(row.get("Designator", "")):
                    assignments.append(Assignment(
                        board=board,
                        reference=reference,
                        value=row.get("Comment", ""),
                        footprint=row.get("Footprint", ""),
                        lcsc=source_part.lcsc,
                        source_refs=source_part.source_refs,
                        status="skipped",
                        reason=reason,
                    ))
                continue

            for reference in split_refs(row.get("Designator", "")):
                assignments.append(Assignment(
                    board=board,
                    reference=reference,
                    value=row.get("Comment", ""),
                    footprint=row.get("Footprint", ""),
                    lcsc=source_part.lcsc,
                    source_refs=source_part.source_refs,
                    status="ready",
                    reason=reason,
                ))

    return assignments


def find_blocks(text: str, marker: str) -> list[tuple[int, int, str]]:
    blocks: list[tuple[int, int, str]] = []
    pos = 0
    while True:
        start = text.find(marker, pos)
        if start < 0:
            break

        depth = 0
        in_string = False
        escaped = False
        end = start

        for end in range(start, len(text)):
            char = text[end]
            if in_string:
                if escaped:
                    escaped = False
                elif char == "\\":
                    escaped = True
                elif char == '"':
                    in_string = False
                continue
            if char == '"':
                in_string = True
            elif char == "(":
                depth += 1
            elif char == ")":
                depth -= 1
                if depth == 0:
                    end += 1
                    blocks.append((start, end, text[start:end]))
                    break
        pos = end
    return blocks


def property_blocks(block: str) -> list[tuple[int, int, str, str]]:
    props: list[tuple[int, int, str, str]] = []
    for start, end, prop in find_blocks(block, "(property "):
        match = re.match(r'\(property\s+"([^"]+)"\s+"((?:\\.|[^"])*)"', prop)
        if match:
            props.append((start, end, match.group(1), match.group(2)))
    return props


def block_reference(block: str) -> str:
    for _, _, name, value in property_blocks(block):
        if name == "Reference":
            return value
    return ""


def escaped(value: str) -> str:
    return value.replace("\\", "\\\\").replace('"', '\\"')


def update_existing_property(prop: str, new_value: str) -> str:
    return re.sub(
        r'(\(property\s+"LCSC Part #"\s+")((?:\\.|[^"])*)(")',
        rf"\g<1>{escaped(new_value)}\3",
        prop,
        count=1,
    )


def pcb_lcsc_property(block: str, lcsc: str) -> str:
    at_match = re.search(r"\n\t\t\(at\s+[-0-9.]+\s+[-0-9.]+(?:\s+([-0-9.]+))?\)", block)
    rotation = at_match.group(1) if at_match and at_match.group(1) else "0"
    return (
        f'\n\t\t(property "LCSC Part #" "{escaped(lcsc)}"\n'
        f"\t\t\t(at 0 0 {rotation})\n"
        '\t\t\t(layer "F.Fab")\n'
        "\t\t\t(hide yes)\n"
        f'\t\t\t(uuid "{uuid.uuid4()}")\n'
        "\t\t\t(effects\n"
        "\t\t\t\t(font\n"
        "\t\t\t\t\t(size 1 1)\n"
        "\t\t\t\t\t(thickness 0.15)\n"
        "\t\t\t\t)\n"
        "\t\t\t)\n"
        "\t\t)"
    )


def sch_lcsc_property(block: str, lcsc: str) -> str:
    at_match = re.search(r"\n\t\t\(at\s+([-0-9.]+)\s+([-0-9.]+)\s+([-0-9.]+)\)", block)
    x, y, rotation = at_match.groups() if at_match else ("0", "0", "0")
    return (
        f'\n\t\t(property "LCSC Part #" "{escaped(lcsc)}"\n'
        f"\t\t\t(at {x} {y} {rotation})\n"
        "\t\t\t(hide yes)\n"
        "\t\t\t(show_name no)\n"
        "\t\t\t(do_not_autoplace no)\n"
        "\t\t\t(effects\n"
        "\t\t\t\t(font\n"
        "\t\t\t\t\t(size 1.27 1.27)\n"
        "\t\t\t\t)\n"
        "\t\t\t)\n"
        "\t\t)"
    )


def update_block(block: str, lcsc: str, kind: str, force: bool) -> tuple[str, str]:
    props = property_blocks(block)
    for start, end, name, value in props:
        if name != "LCSC Part #":
            continue
        if value == lcsc:
            return block, "unchanged"
        if value and not force:
            return block, f"conflict-existing={value}"
        return block[:start] + update_existing_property(block[start:end], lcsc) + block[end:], "updated"

    insert_after = None
    for start, end, name, _ in props:
        if name == "Description":
            insert_after = end
            break
    if insert_after is None and props:
        insert_after = props[-1][1]
    if insert_after is None:
        return block, "skipped-no-property-anchor"

    new_prop = pcb_lcsc_property(block, lcsc) if kind == "pcb" else sch_lcsc_property(block, lcsc)
    return block[:insert_after] + new_prop + block[insert_after:], "added"


def update_file(path: Path, assignments: dict[str, str], kind: str, force: bool) -> dict[str, str]:
    raw = path.read_bytes()
    newline = "\r\n" if b"\r\n" in raw else "\n"
    text = raw.decode("utf-8")
    if newline == "\r\n":
        text = text.replace("\r\n", "\n")
    marker = "(footprint " if kind == "pcb" else "(symbol"
    new_text = text
    actions: dict[str, str] = {}

    for start, end, block in reversed(find_blocks(text, marker)):
        if kind == "sch" and "(lib_id " not in block:
            continue
        reference = block_reference(block)
        lcsc = assignments.get(reference)
        if not lcsc:
            continue

        replacement, action = update_block(block, lcsc, kind, force)
        actions[reference] = action
        if replacement != block:
            new_text = new_text[:start] + replacement + new_text[end:]

    if new_text != text:
        output = new_text.replace("\n", "\r\n") if newline == "\r\n" else new_text
        path.write_bytes(output.encode("utf-8"))
    return actions


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-bom", type=Path, default=Path("deploy/bom.csv"))
    parser.add_argument("--availability-json", type=Path)
    parser.add_argument("--report", type=Path, default=Path("manufacturing/jlcpcb/lcsc-populate-report.csv"))
    parser.add_argument("--force", action="store_true", help="Overwrite existing non-empty LCSC Part # values")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    source, skipped_source = load_source_parts(args.source_bom)
    availability = load_availability(args.availability_json)
    assignments = collect_assignments(source, availability)
    ready_by_board: dict[str, dict[str, str]] = {board: {} for board in BOARDS}

    for assignment in assignments:
        if assignment.status == "ready":
            ready_by_board[assignment.board][assignment.reference] = assignment.lcsc

    action_by_board_ref_kind: dict[tuple[str, str, str], str] = {}
    if not args.dry_run:
        for board, paths in BOARDS.items():
            for kind in ("sch", "pcb"):
                actions = update_file(paths[kind], ready_by_board[board], kind, args.force)
                for reference, action in actions.items():
                    action_by_board_ref_kind[(board, reference, kind)] = action

    rows = []
    for assignment in assignments:
        rows.append({
            "Board": assignment.board,
            "Reference": assignment.reference,
            "Value": assignment.value,
            "Footprint": assignment.footprint,
            "LCSC Part #": assignment.lcsc,
            "Status": assignment.status,
            "Reason": assignment.reason,
            "Schematic Action": action_by_board_ref_kind.get((assignment.board, assignment.reference, "sch"), "dry-run" if args.dry_run and assignment.status == "ready" else ""),
            "PCB Action": action_by_board_ref_kind.get((assignment.board, assignment.reference, "pcb"), "dry-run" if args.dry_run and assignment.status == "ready" else ""),
            "Source Designators": assignment.source_refs,
        })

    for skipped in skipped_source:
        rows.append({
            "Board": "",
            "Reference": "",
            "Value": "",
            "Footprint": "",
            "LCSC Part #": skipped.get("New LCSC", ""),
            "Status": skipped["Status"],
            "Reason": skipped["Reason"],
            "Schematic Action": "",
            "PCB Action": "",
            "Source Designators": skipped["Source Designators"],
        })

    write_csv(args.report, rows, [
        "Board",
        "Reference",
        "Value",
        "Footprint",
        "LCSC Part #",
        "Status",
        "Reason",
        "Schematic Action",
        "PCB Action",
        "Source Designators",
    ])

    ready = sum(1 for item in assignments if item.status == "ready")
    skipped = sum(1 for item in assignments if item.status != "ready") + len(skipped_source)
    print(f"ready={ready} skipped={skipped} report={args.report}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
