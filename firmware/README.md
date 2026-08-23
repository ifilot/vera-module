# VERA FPGA firmware

This directory contains the VERA FPGA source ported to the open-source Yosys,
nextpnr, and Project IceStorm flow.

## Build with Docker

From the repository root, run:

```sh
bash firmware/build-docker.sh
```

The command builds the pinned toolchain image when necessary, synthesizes the
RTL, places and routes it for the iCE40UP5K-SG48, and writes the bitstream to:

```text
firmware/build/vera.bin
```

## Build in an OSS CAD Suite shell

If the pinned OSS CAD Suite is already active, run:

```sh
./firmware/build.sh
```

Generated files and detailed logs are written to `firmware/build/`.

## Download a build from GitHub Actions

Every push or pull request that changes the FPGA firmware or its pinned
toolchain runs the **FPGA firmware** workflow. A successful run publishes the
`vera-fpga-firmware` artifact. Download and extract that artifact to obtain:

- `vera.bin`, the bitstream to flash to VERA;
- `vera.bin.sha256`, its SHA-256 checksum;
- the nextpnr and IceTime timing reports used to accept the build.

The artifact is retained by GitHub for 30 days. A build is not published when
synthesis, place-and-route, or either timing check fails.

## Current timing status

The build enforces the original 25 MHz system-clock constraint twice. nextpnr
must pass its post-route timing analysis, followed by a conservative IceTime
check. A timing failure stops the build before a successful result is reported.

Detailed reports are available in `build/nextpnr.log` and
`build/icetime-conservative.log`.
