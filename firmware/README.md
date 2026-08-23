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
bash firmware/build.sh
```

Generated files and detailed logs are written to `firmware/build/`.

## Firmware identification

The firmware identifies itself with a single null-terminated ASCII string
generated from the repository's root `VERSION` file. Select display-controller
banks 60 through 63 in turn by writing the bank number to bits 6–1 of `CTRL`
(`0x05`). Read registers `0x09` through `0x0C` from each bank and concatenate
the results:

| DCSEL | `0x09` | `0x0A` | `0x0B` | `0x0C` |
|------:|:------:|:------:|:------:|:------:|
| 60 | `V` | `E` | `R` | `A` |
| 61 | space | `v` | `0` | `.` |
| 62 | `1` | `.` | `0` | NUL |
| 63 | NUL | NUL | NUL | NUL |

For the initial ecosystem version, this produces `VERA v0.1.0`. The identifier
can contain at most 15 ASCII characters plus its terminator. Preserve bit 0
(`ADDRSEL`) when changing `DCSEL`, and restore the original `CTRL` value after
reading the identification. Writes to the four display-controller registers
are ignored while one of these identification banks is selected.

## Download a build from GitHub Actions

Every push or pull request that changes the FPGA firmware, ecosystem version,
or pinned toolchain runs the **FPGA firmware** workflow. A successful run
publishes an artifact such as `vera-fpga-firmware-v0.1.0`. Download and extract
that artifact to obtain:

- `vera.bin`, the bitstream to flash to VERA;
- `vera.bin.sha256`, its SHA-256 checksum;
- `VERSION`, the ecosystem version used for the build;
- the nextpnr and IceTime timing reports used to accept the build.

The artifact is retained by GitHub for 30 days. A build is not published when
synthesis, place-and-route, or either timing check fails.

## Current timing status

The build enforces the original 25 MHz system-clock constraint twice. nextpnr
must pass its post-route timing analysis, followed by a conservative IceTime
check. A timing failure stops the build before a successful result is reported.

Detailed reports are available in `build/nextpnr.log` and
`build/icetime-conservative.log`.
