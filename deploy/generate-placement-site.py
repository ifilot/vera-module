#!/usr/bin/env python3
"""Generate a static placement website from a KiCad PCB file."""

from __future__ import annotations

import argparse
import csv
import html
import json
from pathlib import Path


DEFAULT_BOARD = Path("pcb/rev4/vera-module-rev4.kicad_pcb")
DEFAULT_OUTPUT = Path("deploy/site")


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


def child(node: list, name: str) -> list | None:
    for item in node:
        if isinstance(item, list) and item and item[0] == name:
            return item
    return None


def children(node: list, name: str) -> list[list]:
    return [
        item
        for item in node
        if isinstance(item, list) and item and item[0] == name
    ]


def num(value: object, default: float = 0.0) -> float:
    try:
        return float(value)
    except (TypeError, ValueError):
        return default


def xy_from(node: list | None, name: str) -> tuple[float, float] | None:
    item = child(node, name) if node else None
    if not item or len(item) < 3:
        return None
    return (num(item[1]), num(item[2]))


def footprint_property(footprint: list, prop_name: str) -> str:
    for prop in children(footprint, "property"):
        if len(prop) >= 3 and prop[1] == prop_name:
            return str(prop[2])
    return ""


def footprint_kind(reference: str, value: str, pad_types: list[str]) -> str:
    if "smd" in pad_types:
        return "smd"
    if "thru_hole" in pad_types:
        return "thru-hole"
    if "np_thru_hole" in pad_types:
        return "mechanical"
    if reference.startswith("H") or "mount" in value.lower():
        return "mechanical"
    return "other"


def parse_components(pcb: list) -> list[dict]:
    components = []
    for footprint in children(pcb, "footprint"):
        at = child(footprint, "at")
        layer = child(footprint, "layer")
        if not at or len(at) < 3:
            continue

        pads = children(footprint, "pad")
        pad_types = [str(pad[2]) for pad in pads if len(pad) >= 3]
        layer_name = str(layer[1]) if layer and len(layer) >= 2 else ""
        reference = footprint_property(footprint, "Reference")
        value = footprint_property(footprint, "Value")
        library = str(footprint[1]) if len(footprint) >= 2 else ""

        components.append(
            {
                "reference": reference,
                "value": value,
                "footprint": library,
                "x": round(num(at[1]), 4),
                "y": round(num(at[2]), 4),
                "rotation": round(num(at[3]) if len(at) >= 4 else 0.0, 4),
                "layer": layer_name,
                "side": "bottom" if layer_name == "B.Cu" else "top",
                "pad_count": len(pads),
                "kind": footprint_kind(reference, value, pad_types),
            }
        )
    return sorted(components, key=lambda item: natural_ref_key(item["reference"]))


def natural_ref_key(reference: str) -> tuple[str, int, str]:
    prefix = "".join(ch for ch in reference if not ch.isdigit())
    digits = "".join(ch for ch in reference if ch.isdigit())
    return (prefix, int(digits or "0"), reference)


def parse_edges(pcb: list) -> list[dict]:
    edges = []
    for item in pcb:
        if not isinstance(item, list) or not item:
            continue
        if item[0] not in {"gr_line", "gr_arc"}:
            continue
        layer = child(item, "layer")
        if not layer or len(layer) < 2 or layer[1] != "Edge.Cuts":
            continue
        start = xy_from(item, "start")
        end = xy_from(item, "end")
        if not start or not end:
            continue
        edge = {"type": item[0].replace("gr_", ""), "start": start, "end": end}
        mid = xy_from(item, "mid")
        if mid:
            edge["mid"] = mid
        edges.append(edge)
    return edges


def bounding_box(components: list[dict], edges: list[dict]) -> dict:
    points: list[tuple[float, float]] = []
    for edge in edges:
        points.extend([tuple(edge["start"]), tuple(edge["end"])])
        if "mid" in edge:
            points.append(tuple(edge["mid"]))
    if not points:
        points = [(item["x"], item["y"]) for item in components]
    if not points:
        return {"x": 0, "y": 0, "width": 100, "height": 60}

    xs = [point[0] for point in points]
    ys = [point[1] for point in points]
    padding = 4
    min_x = min(xs) - padding
    min_y = min(ys) - padding
    max_x = max(xs) + padding
    max_y = max(ys) + padding
    return {
        "x": round(min_x, 4),
        "y": round(min_y, 4),
        "width": round(max_x - min_x, 4),
        "height": round(max_y - min_y, 4),
    }


def write_csv(path: Path, components: list[dict]) -> None:
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(
            [
                "Designator",
                "Value",
                "Footprint",
                "Mid X (mm)",
                "Mid Y (mm)",
                "Rotation",
                "Layer",
                "Kind",
            ]
        )
        for item in components:
            writer.writerow(
                [
                    item["reference"],
                    item["value"],
                    item["footprint"],
                    item["x"],
                    item["y"],
                    item["rotation"],
                    item["layer"],
                    item["kind"],
                ]
            )


def write_html(path: Path, board_name: str, payload: dict) -> None:
    data = json.dumps(payload, ensure_ascii=True, separators=(",", ":"))
    title = html.escape(board_name)
    path.write_text(
        f"""<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>{title} placement</title>
  <style>
    :root {{
      color-scheme: light;
      --ink: #1b2428;
      --muted: #63727a;
      --line: #cfd8dc;
      --panel: #f7f9fa;
      --board: #2f6f55;
      --board-edge: #12352b;
      --top: #f2c14e;
      --bottom: #7c9eb2;
      --selected: #ef476f;
      --picked: #54a24b;
      font-family: Inter, ui-sans-serif, system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
    }}
    * {{ box-sizing: border-box; }}
    body {{
      margin: 0;
      background: #edf1f3;
      color: var(--ink);
    }}
    header {{
      display: flex;
      flex-wrap: wrap;
      align-items: end;
      gap: 16px;
      padding: 18px 22px 12px;
      background: #ffffff;
      border-bottom: 1px solid var(--line);
    }}
    h1 {{
      margin: 0;
      font-size: 21px;
      line-height: 1.2;
      font-weight: 720;
    }}
    .meta {{
      color: var(--muted);
      font-size: 13px;
    }}
    .controls {{
      display: flex;
      flex-wrap: wrap;
      align-items: center;
      gap: 8px;
      margin-left: auto;
    }}
    input, select, button, a.button {{
      height: 34px;
      border: 1px solid var(--line);
      border-radius: 6px;
      background: #ffffff;
      color: var(--ink);
      font: inherit;
      font-size: 13px;
    }}
    input {{
      width: min(42vw, 260px);
      padding: 0 10px;
    }}
    select {{
      padding: 0 30px 0 10px;
    }}
    button, a.button {{
      display: inline-flex;
      align-items: center;
      justify-content: center;
      padding: 0 10px;
      text-decoration: none;
      cursor: pointer;
    }}
    main {{
      display: grid;
      grid-template-columns: minmax(360px, 1fr) minmax(360px, 43vw);
      min-height: calc(100vh - 67px);
    }}
    .board-pane {{
      padding: 16px;
      min-width: 0;
    }}
    .board-wrap {{
      height: calc(100vh - 99px);
      min-height: 420px;
      background: #ffffff;
      border: 1px solid var(--line);
      border-radius: 8px;
      overflow: hidden;
    }}
    svg {{
      display: block;
      width: 100%;
      height: 100%;
      background: #f9fbfb;
    }}
    .board-fill {{
      fill: color-mix(in srgb, var(--board) 18%, white);
      stroke: none;
    }}
    .edge {{
      fill: none;
      stroke: var(--board-edge);
      stroke-width: .22;
      vector-effect: non-scaling-stroke;
    }}
    .footprint {{
      cursor: pointer;
    }}
    .footprint rect, .footprint circle {{
      stroke: #24292f;
      stroke-width: .18;
      vector-effect: non-scaling-stroke;
    }}
    .footprint.top rect, .footprint.top circle {{ fill: var(--top); }}
    .footprint.bottom rect, .footprint.bottom circle {{ fill: var(--bottom); }}
    .footprint.hidden {{ display: none; }}
    .footprint.picked rect, .footprint.picked circle {{ fill: var(--picked); }}
    .footprint.selected rect, .footprint.selected circle {{
      fill: var(--selected);
      stroke-width: .32;
    }}
    .ref-label {{
      pointer-events: none;
      font-size: 2.1px;
      text-anchor: middle;
      dominant-baseline: central;
      fill: #111;
    }}
    .table-pane {{
      background: #ffffff;
      border-left: 1px solid var(--line);
      overflow: auto;
      max-height: calc(100vh - 67px);
    }}
    table {{
      width: 100%;
      border-collapse: collapse;
      table-layout: fixed;
      font-size: 13px;
    }}
    thead {{
      position: sticky;
      top: 0;
      z-index: 2;
      background: #ffffff;
      box-shadow: 0 1px 0 var(--line);
    }}
    th, td {{
      text-align: left;
      padding: 8px 10px;
      border-bottom: 1px solid #e6ecef;
      white-space: nowrap;
      overflow: hidden;
      text-overflow: ellipsis;
    }}
    th {{
      color: var(--muted);
      font-size: 11px;
      text-transform: uppercase;
      letter-spacing: 0;
      font-weight: 700;
    }}
    tr {{
      cursor: pointer;
    }}
    tr:hover, tr.selected {{
      background: #f2f6f7;
    }}
    tr.picked {{
      background: #eef7ed;
    }}
    .check {{
      width: 34px;
      text-align: center;
    }}
    .ref {{
      width: 72px;
      font-weight: 700;
    }}
    .side, .kind, .rot {{
      width: 80px;
    }}
    .xy {{
      width: 98px;
      color: var(--muted);
      font-variant-numeric: tabular-nums;
    }}
    @media (max-width: 900px) {{
      header {{
        align-items: stretch;
      }}
      .controls {{
        margin-left: 0;
        width: 100%;
      }}
      input {{
        width: 100%;
        flex: 1 1 180px;
      }}
      main {{
        display: block;
      }}
      .board-wrap {{
        height: 56vh;
      }}
      .table-pane {{
        max-height: none;
        border-left: 0;
        border-top: 1px solid var(--line);
      }}
    }}
  </style>
</head>
<body>
  <header>
    <div>
      <h1>{title} placement</h1>
      <div class="meta"><span id="visible-count"></span> visible, <span id="picked-count"></span> picked</div>
    </div>
    <div class="controls">
      <input id="search" type="search" placeholder="Reference, value, footprint">
      <select id="side">
        <option value="all">Both sides</option>
        <option value="top">Top</option>
        <option value="bottom">Bottom</option>
      </select>
      <select id="kind">
        <option value="all">All parts</option>
        <option value="smd">SMD</option>
        <option value="thru-hole">Thru-hole</option>
        <option value="mechanical">Mechanical</option>
      </select>
      <button id="clear-picked" type="button">Clear picks</button>
      <a class="button" href="placements.csv" download>CSV</a>
    </div>
  </header>
  <main>
    <section class="board-pane">
      <div class="board-wrap">
        <svg id="board" role="img" aria-label="{title} placement map"></svg>
      </div>
    </section>
    <section class="table-pane">
      <table>
        <thead>
          <tr>
            <th class="check"></th>
            <th class="ref">Ref</th>
            <th>Value</th>
            <th>Footprint</th>
            <th class="side">Side</th>
            <th class="kind">Kind</th>
            <th class="xy">X/Y</th>
            <th class="rot">Rot</th>
          </tr>
        </thead>
        <tbody id="rows"></tbody>
      </table>
    </section>
  </main>
  <script id="placement-data" type="application/json">{data}</script>
  <script>
    const data = JSON.parse(document.getElementById("placement-data").textContent);
    const components = data.components;
    const pickedKey = `placement-picked:${{data.board}}`;
    const picked = new Set(JSON.parse(localStorage.getItem(pickedKey) || "[]"));
    let selectedRef = null;

    const svg = document.getElementById("board");
    const rows = document.getElementById("rows");
    const search = document.getElementById("search");
    const side = document.getElementById("side");
    const kind = document.getElementById("kind");
    const visibleCount = document.getElementById("visible-count");
    const pickedCount = document.getElementById("picked-count");

    function savePicked() {{
      localStorage.setItem(pickedKey, JSON.stringify([...picked].sort()));
    }}

    function footprintSize(component) {{
      if (component.kind === "mechanical") return [2.8, 2.8, "circle"];
      if (component.pad_count >= 40) return [8.6, 8.6, "rect"];
      if (component.pad_count >= 12) return [6.8, 4.4, "rect"];
      if (component.pad_count >= 3) return [4.4, 2.8, "rect"];
      return [2.8, 1.45, "rect"];
    }}

    function edgePath(edge) {{
      const [sx, sy] = edge.start;
      const [ex, ey] = edge.end;
      if (edge.type === "arc" && edge.mid) {{
        const [mx, my] = edge.mid;
        const cx = 2 * mx - 0.5 * sx - 0.5 * ex;
        const cy = 2 * my - 0.5 * sy - 0.5 * ey;
        return `M ${{sx}} ${{sy}} Q ${{cx}} ${{cy}} ${{ex}} ${{ey}}`;
      }}
      return `M ${{sx}} ${{sy}} L ${{ex}} ${{ey}}`;
    }}

    function createSvg(name, attrs = {{}}) {{
      const node = document.createElementNS("http://www.w3.org/2000/svg", name);
      for (const [key, value] of Object.entries(attrs)) node.setAttribute(key, value);
      return node;
    }}

    function renderBoard() {{
      svg.textContent = "";
      const box = data.bbox;
      svg.setAttribute("viewBox", `${{box.x}} ${{box.y}} ${{box.width}} ${{box.height}}`);

      svg.appendChild(createSvg("rect", {{
        class: "board-fill",
        x: box.x + 4,
        y: box.y + 4,
        width: box.width - 8,
        height: box.height - 8,
        rx: 3.5,
      }}));

      for (const edge of data.edges) {{
        svg.appendChild(createSvg("path", {{ class: "edge", d: edgePath(edge) }}));
      }}

      for (const component of components) {{
        const [w, h, shape] = footprintSize(component);
        const group = createSvg("g", {{
          class: `footprint ${{component.side}}`,
          transform: `translate(${{component.x}} ${{component.y}}) rotate(${{component.rotation}})`,
          "data-ref": component.reference,
        }});
        if (shape === "circle") {{
          group.appendChild(createSvg("circle", {{ cx: 0, cy: 0, r: w / 2 }}));
        }} else {{
          group.appendChild(createSvg("rect", {{ x: -w / 2, y: -h / 2, width: w, height: h, rx: .35 }}));
        }}
        const label = createSvg("text", {{ class: "ref-label", x: 0, y: 0 }});
        label.textContent = component.reference;
        group.appendChild(label);
        group.addEventListener("click", () => selectRef(component.reference));
        svg.appendChild(group);
      }}
    }}

    function matches(component) {{
      const query = search.value.trim().toLowerCase();
      const text = `${{component.reference}} ${{component.value}} ${{component.footprint}}`.toLowerCase();
      return (!query || text.includes(query))
        && (side.value === "all" || component.side === side.value)
        && (kind.value === "all" || component.kind === kind.value);
    }}

    function renderRows() {{
      rows.textContent = "";
      let visible = 0;
      for (const component of components) {{
        if (!matches(component)) continue;
        visible += 1;
        const row = document.createElement("tr");
        row.dataset.ref = component.reference;
        row.innerHTML = `
          <td class="check"><input type="checkbox" ${{picked.has(component.reference) ? "checked" : ""}}></td>
          <td class="ref">${{component.reference}}</td>
          <td title="${{component.value}}">${{component.value}}</td>
          <td title="${{component.footprint}}">${{component.footprint.split(":").pop()}}</td>
          <td class="side">${{component.side}}</td>
          <td class="kind">${{component.kind}}</td>
          <td class="xy">${{component.x}}, ${{component.y}}</td>
          <td class="rot">${{component.rotation}}</td>
        `;
        row.querySelector("input").addEventListener("click", event => {{
          event.stopPropagation();
          togglePicked(component.reference);
        }});
        row.addEventListener("click", () => selectRef(component.reference));
        rows.appendChild(row);
      }}
      visibleCount.textContent = visible;
      pickedCount.textContent = picked.size;
      syncClasses();
    }}

    function syncClasses() {{
      document.querySelectorAll(".footprint").forEach(node => {{
        const component = components.find(item => item.reference === node.dataset.ref);
        node.classList.toggle("hidden", !component || !matches(component));
        node.classList.toggle("selected", node.dataset.ref === selectedRef);
        node.classList.toggle("picked", picked.has(node.dataset.ref));
      }});
      rows.querySelectorAll("tr").forEach(row => {{
        row.classList.toggle("selected", row.dataset.ref === selectedRef);
        row.classList.toggle("picked", picked.has(row.dataset.ref));
      }});
      pickedCount.textContent = picked.size;
    }}

    function selectRef(reference) {{
      selectedRef = reference;
      syncClasses();
      const row = rows.querySelector(`tr[data-ref="${{CSS.escape(reference)}}"]`);
      if (row) row.scrollIntoView({{ block: "nearest" }});
    }}

    function togglePicked(reference) {{
      if (picked.has(reference)) picked.delete(reference);
      else picked.add(reference);
      savePicked();
      renderRows();
    }}

    search.addEventListener("input", renderRows);
    side.addEventListener("change", renderRows);
    kind.addEventListener("change", renderRows);
    document.getElementById("clear-picked").addEventListener("click", () => {{
      picked.clear();
      savePicked();
      renderRows();
    }});

    renderBoard();
    renderRows();
  </script>
</body>
</html>
""",
        encoding="utf-8",
    )


def generate(board: Path, output: Path) -> None:
    pcb = parse_sexpr(tokenize(board.read_text(encoding="utf-8")))
    components = parse_components(pcb)
    edges = parse_edges(pcb)
    payload = {
        "board": board.name,
        "components": components,
        "edges": edges,
        "bbox": bounding_box(components, edges),
    }

    output.mkdir(parents=True, exist_ok=True)
    write_csv(output / "placements.csv", components)
    write_html(output / "index.html", board.name, payload)
    print(f"Generated {output / 'index.html'}")
    print(f"Generated {output / 'placements.csv'}")
    print(f"Components: {len(components)}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--board", type=Path, default=DEFAULT_BOARD)
    parser.add_argument("--out", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()
    generate(args.board, args.out)


if __name__ == "__main__":
    main()
