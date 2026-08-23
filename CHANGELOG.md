# Changelog

This project is a maintained fork of Frank van den Hoef's VERA Module.  The
baseline for the entries below is upstream commit `fa40dbe` (2023-10-20), the
last identified upstream commit before this fork's maintenance series.  The
original project history is preserved in Git and is not rewritten here.

## 0.1.0 — 2026-08-23

### Hardware and manufacturing

- Added maintained KiCad board variants based on the earlier VERA module
  designs: `vera-full-audiojack` (formerly `fork-r0`) and
  `vera-mini-audiojack` (formerly `fork-r1`).
- Both variants add an audio jack and a small SD card slot.  The mini variant
  is a reduced-size design without the RCA and composite-video jacks.
- Refined the mini design and added component-identification work.
- Added and iterated manufacturing/placement automation during the fork's
  board-development work; generated and superseded artifacts are intentionally
  not treated as source of truth.

### FPGA firmware and build

- Moved the maintained FPGA source tree to `firmware/` and ported its build to
  the open-source Yosys/nextpnr/iCE40 toolchain.
- Added portable palette and sprite RAM implementations and updated the
  surrounding HDL, pin constraints, synthesis configuration, and build
  scripts for that toolchain.
- Added a Docker-based, pinned open-source CAD environment plus a small build
  example and test path.
- Added firmware build/release automation and version-derived FPGA
  identification.

### Programmer, driver, and tooling

- Added an open-source Raspberry Pi Pico programmer, its command-line tool,
  firmware, and associated KiCad design files.
- Synchronized the programmer firmware with the PCB's READ and WRITE LEDs,
  added bounded flash-operation failure handling, and made the CLI verify the
  target W25Q16 JEDEC identity before erasing.
- Added version-aware programmer build and release automation.
- Added the Arduino Mega driver/demo, including font assets and build
  instructions, to the maintained repository.

### Project maintenance

- Added a single repository `VERSION` source of truth and `VERSIONING.md` for
  the FPGA, programmer, and release artifacts.
- Removed legacy vendor-tool build material and historical binaries/documents
  that are not part of the maintained build path.
- Established the licensing and attribution policy.  Historic MIT notices
  remain in force; the reciprocal licensing of maintained-fork contributions
  is described in [LICENSING.md](LICENSING.md).

## Compatibility commitment

The fork aims to remain compatible with Frank van den Hoef's original VERA
design where practical.  Compatibility is a project goal, not a statement of
endorsement by the original author or a guarantee that every future board,
firmware image, or tool release is interchangeable.  Release notes should
identify any intentional compatibility change.
