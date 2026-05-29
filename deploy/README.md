# Placement site

Generate static placement websites for the two fork boards:

```sh
./deploy/generate-placement-site.sh
```

The output is written to `deploy/site/`:

- `index.html`: links to the available fork placement views
- `fork-r0/index.html`: interactive placement view for `fork-r0`
- `fork-r0/placements.csv`: pick-and-place style placement table for `fork-r0`
- `fork-r1/index.html`: interactive placement view for `fork-r1`
- `fork-r1/placements.csv`: pick-and-place style placement table for `fork-r1`

The default build intentionally includes only:

- `pcb/fork-r0/fork-r0.kicad_pcb`
- `pcb/fork-r1/fork-r1.kicad_pcb`

Use a different single board or output folder for local debugging with:

```sh
./deploy/generate-placement-site.sh \
  --board pcb/fork-r1/fork-r1.kicad_pcb \
  --out deploy/site
```

The shell script prefers existing KiCad ecosystem tools:

- Docker image `ghcr.io/inti-cmnb/kicad10_auto:latest`, which contains KiCad,
  KiBot, and InteractiveHtmlBom
- Local KiBot or InteractiveHtmlBom, when available directly
- Local KiCad CLI for `placements.csv`

If Docker and the local tools are not installed, it falls back to a small built-in
preview generator so the site can still be inspected locally. That fallback is
not a replacement for the Dockerized render; it intentionally draws less board
detail.
