#!/usr/bin/env bash
set -euo pipefail

firmware_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${firmware_dir}"

mkdir -p build

yosys -l build/yosys.log vera.ys

nextpnr-ice40 \
    --up5k \
    --package sg48 \
    --ignore-rel-clk \
    --opt-timing \
    --placer-heap-critexp 4 \
    --placer-heap-timingweight 30 \
    --seed 3 \
    --json build/vera.json \
    --pcf source/vera.pcf \
    --asc build/vera.asc \
    --report build/vera-report.json \
    --log build/nextpnr.log

icetime \
    -d up5k \
    -P sg48 \
    -p source/vera.pcf \
    -i \
    -t \
    -m \
    -c 25 \
    -r build/icetime-conservative.log \
    build/vera.asc

icepack build/vera.asc build/vera.bin

test -s build/vera.bin
sha256sum build/vera.bin
echo "PASS: generated firmware/build/vera.bin"
echo "PASS: nextpnr timing requirements satisfied"
echo "PASS: conservative IceTime timing requirement satisfied"
