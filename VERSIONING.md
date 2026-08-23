# Versioning

`VERSION` at the repository root is the single source of truth for the VERA
ecosystem version. It contains a semantic version without a leading `v`, for
example `0.1.0`. User-facing identifiers and artifact names add the `v` prefix.

The version is propagated to:

- the identification string exposed by the VERA FPGA;
- the Raspberry Pi Pico programmer identity;
- the `vera-flash` command-line tool;
- GitHub Actions artifact names and their included `VERSION` files.

Toolchain and dependency versions, such as the OSS CAD Suite release and Pico
SDK version, are pinned independently and do not represent the VERA ecosystem
version.
