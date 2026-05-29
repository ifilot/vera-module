# VERA Module Fork

This repository is a fork of the VERA module, the Video Embedded Retro Adapter
developed for the Commander X16 computer. It contains the PCB designs, FPGA
source, firmware, programmer utilities, documentation, and manufacturing support
files for the module.

This fork keeps the original project history available while adding forked PCB
variants that can be reviewed, rendered, and published independently from the
upstream Commander X16-branded board files.

## What Changed In This Fork

- Added `pcb/fork-r0/` as the first forked board variant.
- Added `pcb/fork-r1/` as the current forked board variant.
- Removed/reworked branding that should not be reused outside the Commander X16
  project.
- Migrated the fork boards to modern KiCad project files.
- Added local symbol/cache fixes for migrated KiCad schematics so parts render
  and connect as intended.
- Added Docker-based placement-site generation for `fork-r0` and `fork-r1`.
- Added a GitHub Pages workflow that can publish the generated placement site.

The older upstream board revisions remain under `pcb/rev0/` through `pcb/rev4/`
for reference.

## Repository Layout

- `pcb/fork-r0/`: first forked PCB variant.
- `pcb/fork-r1/`: current forked PCB variant.
- `pcb/rev*/`: original/upstream board revisions.
- `fpga/`: VERA FPGA source and project files.
- `programmer/stm32/`: STM32 firmware for the programmer.
- `programmer/programmer_tool/`: host-side programmer tool.
- `doc/`: VERA documentation and datasheets.
- `deploy/`: scripts and configuration for generated placement websites.

## Placement Website

Generate the placement website locally with:

```sh
./deploy/generate-placement-site.sh
```

The default build intentionally renders only the two fork boards:

- `pcb/fork-r0/fork-r0.kicad_pcb`
- `pcb/fork-r1/fork-r1.kicad_pcb`

The generated output is written to:

- `deploy/site/index.html`
- `deploy/site/fork-r0/index.html`
- `deploy/site/fork-r0/placements.csv`
- `deploy/site/fork-r1/index.html`
- `deploy/site/fork-r1/placements.csv`

The script prefers the Docker image `ghcr.io/inti-cmnb/kicad10_auto:latest`,
which contains KiCad, KiBot, and InteractiveHtmlBom. If Docker is unavailable, it
falls back to local KiBot/InteractiveHtmlBom tools, and finally to a limited
built-in preview generator.

## JLCPCB Assembly Files

Generate JLCPCB-style SMT assembly files with:

```sh
./deploy/generate-jlcpcb-files.sh
```

The generated files are written to:

- `manufacturing/jlcpcb/fork-r0/bom.csv`
- `manufacturing/jlcpcb/fork-r0/cpl.csv`
- `manufacturing/jlcpcb/fork-r0/missing_lcsc.csv`
- `manufacturing/jlcpcb/fork-r1/bom.csv`
- `manufacturing/jlcpcb/fork-r1/cpl.csv`
- `manufacturing/jlcpcb/fork-r1/missing_lcsc.csv`

The BOM contains `Comment`, `Designator`, `Footprint`, and `LCSC Part #`
columns. The CPL contains `Designator`, `Mid X`, `Mid Y`, `Layer`, and
`Rotation` columns. The exporter includes populated SMD footprints and skips
mounting holes, fiducials, logos, DNP parts, and footprints marked as excluded
from BOM or position files.

`missing_lcsc.csv` is a review file listing parts that still need a JLCPCB/LCSC
part number before automated assembly ordering.

You can also generate live JLCPCB part candidates:

```sh
ALLOW_JLCPCB_LOOKUP=1 ./deploy/find-jlcpcb-parts.sh
```

This sends BOM-derived search terms to `jlcpcb.com`, so it requires explicit
opt-in. It writes `bom_autofilled.csv` and `part_candidates.csv` beside each
board's BOM. The candidate finder prefers in-stock Basic/common parts and lower
prices, but the generated selections should still be reviewed before ordering.

## CI/CD

The GitHub Actions workflow in `.github/workflows/placement-site.yml` builds the
placement site for `fork-r0` and `fork-r1`. On `main` or `master`, it can publish
the generated `deploy/site/` output to GitHub Pages.

The workflow is triggered by changes to:

- `.github/workflows/placement-site.yml`
- `deploy/**`
- `pcb/fork-r0/**`
- `pcb/fork-r1/**`

## Building Source Code

The STM32 programmer firmware can be built from `programmer/stm32/`:

```sh
make -C programmer/stm32
```

The host-side programmer tool lives in `programmer/programmer_tool/` and depends
on libusb development headers, for example `libusb-1.0-0-dev` on Debian/Ubuntu.

The FPGA source is under `fpga/`. The checked-in project targets the Lattice
iCE40UP5K device family and is intended to be built with the Lattice toolchain
used by the upstream VERA project.

## Branding Note

The Commander X16 logo belongs to the Commander X16 project. Do not reuse that
logo for unrelated boards, forks, or derivative projects unless you have explicit
permission.
