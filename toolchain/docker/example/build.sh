#!/usr/bin/env bash
set -euo pipefail

example_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${example_dir}"

mkdir -p build

iverilog \
    -g2005 \
    -s tb_blinky \
    -o build/blinky_tb \
    blinky.v \
    tb_blinky.v
simulation_output="$(vvp build/blinky_tb)"
echo "${simulation_output}"
grep -q '^PASS: blinky simulation$' <<<"${simulation_output}"

yosys \
    -q \
    -l build/yosys.log \
    -p 'read_verilog blinky.v; synth_ice40 -top blinky -json build/blinky.json'

nextpnr-ice40 \
    --up5k \
    --package sg48 \
    --json build/blinky.json \
    --pcf blinky.pcf \
    --asc build/blinky.asc \
    --report build/blinky-report.json \
    --log build/nextpnr.log

icepack build/blinky.asc build/blinky.bin

test -s build/blinky.bin
sha256sum build/blinky.bin
echo "PASS: generated build/blinky.bin"
