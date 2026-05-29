#!/usr/bin/env bash
set -euo pipefail

board=""
out="manufacturing"
image="${MANUFACTURING_KICAD_IMAGE:-${PLACEMENT_KICAD_IMAGE:-ghcr.io/inti-cmnb/kicad10_auto:latest}}"
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_dir="$(cd "$script_dir/.." && pwd)"
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

case "$out" in
  /*)
    echo "--out must be a path relative to the repository root" >&2
    exit 2
    ;;
esac

cd "$repo_dir"
uid="$(id -u)"
gid="$(id -g)"
gerber_layers="F.Cu,B.Cu,F.Paste,B.Paste,F.SilkS,B.SilkS,F.Mask,B.Mask,Edge.Cuts"

docker_run() {
  docker run --rm \
    -u "$uid:$gid" \
    -e HOME=/tmp \
    -v "$repo_dir:/work" \
    -w /work \
    "$image" \
    "$@"
}

kicad_cli() {
  if command -v docker >/dev/null 2>&1; then
    docker_run kicad-cli "$@"
  elif command -v kicad-cli >/dev/null 2>&1; then
    kicad-cli "$@"
  else
    echo "Docker or kicad-cli is required to generate Gerber files" >&2
    exit 1
  fi
}

kicad_output_path() {
  if command -v docker >/dev/null 2>&1; then
    printf '/work/%s' "$1"
  else
    printf '%s' "$1"
  fi
}

reset_generated_dir() {
  local path="$1"

  case "$path" in
    "$out"/gerbers/*)
      rm -rf "$path"
      mkdir -p "$path"
      ;;
    *)
      echo "Refusing to clean unexpected generated path: $path" >&2
      exit 1
      ;;
  esac
}

zip_dir() {
  local src_dir="$1"
  local zip_path="$2"

  rm -f "$zip_path"
  if command -v zip >/dev/null 2>&1; then
    (
      cd "$src_dir"
      shopt -s nullglob
      files=( *.gbr *.gbrjob *.gm1 *.gtl *.gbl *.gtp *.gbp *.gto *.gbo *.gts *.gbs *.drl )
      if [ "${#files[@]}" -eq 0 ]; then
        echo "No Gerber or drill files found in $src_dir" >&2
        exit 1
      fi
      zip -q "../$(basename "$zip_path")" "${files[@]}"
    )
  else
    python3 - "$src_dir" "$zip_path" <<'PY'
from pathlib import Path
import sys
import zipfile

src = Path(sys.argv[1])
zip_path = Path(sys.argv[2])
suffixes = {".gbr", ".gbrjob", ".gm1", ".gtl", ".gbl", ".gtp", ".gbp", ".gto", ".gbo", ".gts", ".gbs", ".drl"}
matches = [path for path in sorted(src.iterdir()) if path.is_file() and path.suffix.lower() in suffixes]
if not matches:
    raise SystemExit(f"No Gerber or drill files found in {src}")
with zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_DEFLATED) as archive:
    for path in matches:
        archive.write(path, path.name)
PY
  fi
}

generate_gerbers() {
  local name="$1"
  local board_path="$2"
  local board_out="$out/gerbers/$name"
  local zip_path="$out/gerbers/$name.zip"
  local cli_board_out
  local cli_report_path

  reset_generated_dir "$board_out"
  cli_board_out="$(kicad_output_path "$board_out")"
  cli_report_path="$(kicad_output_path "$board_out/$name-drill-report.rpt")"

  echo "Generating Gerbers for $name"
  kicad_cli pcb export gerbers \
    --output "$cli_board_out" \
    --layers "$gerber_layers" \
    --subtract-soldermask \
    --check-zones \
    "$board_path"

  echo "Generating drill files for $name"
  kicad_cli pcb export drill \
    --output "$cli_board_out" \
    --format excellon \
    --excellon-units mm \
    --generate-map \
    --generate-report \
    --report-path "$cli_report_path" \
    "$board_path"

  zip_dir "$board_out" "$zip_path"
}

generate_board() {
  local entry="$1"
  local name="${entry%%:*}"
  local board_path="${entry#*:}"

  generate_gerbers "$name" "$board_path"
}

if [ -n "$board" ]; then
  matched=""
  for entry in "${boards[@]}"; do
    name="${entry%%:*}"
    if [ "$board" = "$name" ]; then
      matched="$entry"
      break
    fi
  done

  if [ -z "$matched" ]; then
    echo "Unknown board: $board" >&2
    exit 2
  fi

  generate_board "$matched"
  echo "Generating JLCPCB BOM/CPL files"
  python3 deploy/generate-jlcpcb-files.py --board "$matched" --out "$out/jlcpcb"
else
  mkdir -p "$out/gerbers"
  for entry in "${boards[@]}"; do
    generate_board "$entry"
  done
  echo "Generating JLCPCB BOM/CPL files"
  python3 deploy/generate-jlcpcb-files.py --out "$out/jlcpcb"
fi

echo "Manufacturing files are ready in $out"
