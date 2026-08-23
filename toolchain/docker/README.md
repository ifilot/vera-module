# Build the VERA FPGA sources with Docker

This directory provides the supported open-source FPGA build environment for
the VERA module. It contains a pinned OSS CAD Suite with the tools required to
simulate Verilog and generate an iCE40UP5K bitstream:

- Icarus Verilog
- Yosys
- nextpnr-ice40
- Project IceStorm

Docker keeps these tools separate from the host system and ensures that every
user builds with the same versions. Images are available for Linux x86-64 and
ARM64 hosts.

## Requirements

Install Docker Engine or Docker Desktop and make sure it is running. Windows
users should enable Docker Desktop integration for the WSL distribution that
contains this repository.

Allow roughly 700 MB for the initial OSS CAD Suite download. Docker caches this
layer, so later builds do not normally download it again.

## Verify the toolchain

From the repository root, run:

```sh
./toolchain/docker/test.sh
```

This command builds the image and runs a small design through the same stages
used for VERA:

```text
Verilog simulation -> synthesis -> place and route -> bitstream generation
```

The test targets the VERA module's `iCE40UP5K-SG48` FPGA. A successful run ends
with two `PASS` messages and creates:

```text
toolchain/docker/example/build/blinky.bin
```

The example confirms that Icarus Verilog, Yosys, nextpnr-ice40, the UP5K device
database, the SG48 package definition, and `icepack` all work together.

## Open a toolchain shell

To use the FPGA tools directly, first build the image:

```sh
docker build -t vera-oss-cad:2026-08-23 toolchain/docker
```

Then start a shell with the repository mounted at `/workspace`:

```sh
docker run --rm -it \
  --user "$(id -u):$(id -g)" \
  -e HOME=/tmp \
  -v "$PWD:/workspace" \
  -w /workspace \
  vera-oss-cad:2026-08-23
```

Commands such as `iverilog`, `yosys`, `nextpnr-ice40`, and `icepack` are
available on `PATH` inside this shell. Files generated below `/workspace` remain
in the checked-out repository and are owned by the host user.

## Build the VERA firmware

The open-source VERA firmware is available under `firmware`. From the repository
root, build it with:

```sh
./firmware/build-docker.sh
```

The build generates `firmware/build/vera.bin` and fails if either nextpnr or the
conservative IceTime check does not satisfy the 25 MHz clock requirement.

FPGA programming is performed on the host with the existing VERA programmer
utility. It accepts the raw `.bin` produced by `icepack`, and keeping it outside
Docker avoids USB passthrough and device-permission issues.
