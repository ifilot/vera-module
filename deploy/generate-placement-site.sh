#!/usr/bin/env bash
set -euo pipefail

board=""
out="deploy/site"
image="${PLACEMENT_KICAD_IMAGE:-ghcr.io/inti-cmnb/kicad10_auto:latest}"
boards=(
  "fork-r0:pcb/fork-r0/fork-r0.kicad_pcb"
  "fork-r1:pcb/fork-r1/fork-r1.kicad_pcb"
)

while [ "$#" -gt 0 ]; do
  case "$1" in
    --board)
      board="$2"
      shift 2
      ;;
    --out)
      out="$2"
      shift 2
      ;;
    *)
      echo "Unknown argument: $1" >&2
      exit 2
      ;;
  esac
done

mkdir -p "$out"

repo_dir="$(pwd)"
uid="$(id -u)"
gid="$(id -g)"

docker_run() {
  docker run --rm \
    -u "$uid:$gid" \
    -e HOME=/tmp \
    -v "$repo_dir:/work" \
    -w /work \
    "$image" \
    "$@"
}

docker_shell() {
  docker_run sh -lc "$1"
}

write_index() {
  local index="$out/index.html"
  {
    printf '%s\n' '<!doctype html>'
    printf '%s\n' '<html lang="en">'
    printf '%s\n' '<head>'
    printf '%s\n' '  <meta charset="utf-8">'
    printf '%s\n' '  <meta name="viewport" content="width=device-width, initial-scale=1">'
    printf '%s\n' '  <title>VERA fork placement sites</title>'
    printf '%s\n' '  <style>'
    printf '%s\n' '    :root { color-scheme: light dark; font-family: system-ui, sans-serif; }'
    printf '%s\n' '    body { margin: 0; min-height: 100vh; display: grid; place-items: center; background: Canvas; color: CanvasText; }'
    printf '%s\n' '    main { width: min(680px, calc(100vw - 40px)); }'
    printf '%s\n' '    h1 { font-size: clamp(2rem, 7vw, 4rem); line-height: 1; margin: 0 0 1.5rem; letter-spacing: 0; }'
    printf '%s\n' '    ul { display: grid; gap: 0.75rem; padding: 0; margin: 0; list-style: none; }'
    printf '%s\n' '    a { display: flex; justify-content: space-between; gap: 1rem; padding: 1rem; border: 1px solid color-mix(in srgb, CanvasText 22%, transparent); border-radius: 8px; color: inherit; text-decoration: none; }'
    printf '%s\n' '    a:hover, a:focus-visible { border-color: CanvasText; outline: none; }'
    printf '%s\n' '    span { opacity: 0.68; }'
    printf '%s\n' '  </style>'
    printf '%s\n' '</head>'
    printf '%s\n' '<body>'
    printf '%s\n' '  <main>'
    printf '%s\n' '    <h1>Placement</h1>'
    printf '%s\n' '    <ul>'
    for entry in "${boards[@]}"; do
      local name="${entry%%:*}"
      local board_path="${entry#*:}"
      printf '      <li><a href="%s/"><strong>%s</strong><span>%s</span></a></li>\n' "$name" "$name" "$board_path"
    done
    printf '%s\n' '    </ul>'
    printf '%s\n' '  </main>'
    printf '%s\n' '</body>'
    printf '%s\n' '</html>'
  } > "$index"
}

generate_board() {
  local board_path="$1"
  local board_out="$2"

  mkdir -p "$board_out"

  if command -v docker >/dev/null 2>&1; then
    echo "Generating placement website with Dockerized InteractiveHtmlBom for $board_path"
    docker_shell "Xvfb :99 -screen 0 1280x1024x24 >/tmp/xvfb.log 2>&1 & export DISPLAY=:99; generate_interactive_bom.py --no-browser --dest-dir /work/$board_out --name-format index $board_path"

    echo "Generating position CSV with Dockerized KiCad CLI for $board_path"
    docker_run kicad-cli pcb export pos \
      --output "/work/$board_out/placements.csv" \
      --format csv \
      --units mm \
      --side both \
      "$board_path"
  elif command -v kibot >/dev/null 2>&1; then
    echo "Generating placement website with local KiBot for $board_path"
    kibot -c deploy/kibot-placement.yml -b "$board_path" -d "$board_out"
  elif command -v generate_interactive_bom.py >/dev/null 2>&1; then
    echo "Generating placement website with local InteractiveHtmlBom for $board_path"
    generate_interactive_bom.py \
      --no-browser \
      --dest-dir "$board_out" \
      --name-format index \
      "$board_path"
  elif command -v generate_interactive_bom >/dev/null 2>&1; then
    echo "Generating placement website with local InteractiveHtmlBom for $board_path"
    generate_interactive_bom \
      --no-browser \
      --dest-dir "$board_out" \
      --name-format index \
      "$board_path"
  else
    echo "Docker/KiBot/InteractiveHtmlBom not found; generating limited built-in preview for $board_path"
    python3 deploy/generate-placement-site.py --board "$board_path" --out "$board_out"
  fi

  if ! command -v docker >/dev/null 2>&1 && command -v kicad-cli >/dev/null 2>&1; then
    echo "Generating position CSV with KiCad CLI for $board_path"
    kicad-cli pcb export pos \
      --output "$board_out/placements.csv" \
      --format csv \
      --units mm \
      --side both \
      "$board_path"
  elif [ ! -f "$board_out/placements.csv" ]; then
    echo "placements.csv was not generated and kicad-cli is unavailable" >&2
    exit 1
  fi
}

if [ -n "$board" ]; then
  generate_board "$board" "$out"
else
  rm -f "$out/placements.csv"
  write_index
  for entry in "${boards[@]}"; do
    name="${entry%%:*}"
    board_path="${entry#*:}"
    generate_board "$board_path" "$out/$name"
  done
fi

echo "Placement site is ready in $out"
