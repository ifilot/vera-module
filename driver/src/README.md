# VERA Arduino Mega bridge

The Arduino Mega 2560 is a USB-serial bridge to VERA's 8-bit parallel bus.
It is intentionally not an autonomous graphics driver: image conversion,
palette handling, display configuration, and diagnostics live in the Qt GUI at
`driver/gui/`.

Connect the Mega as follows:

- Mega 22..29 to VERA D0..D7;
- Mega 30..34 to VERA A0..A4;
- Mega 35, 36, and 37 to VERA `/CS`, `/RD`, and `/WR` respectively;
- Mega 38 to VERA `/RESET`;
- a shared ground, with suitable 3.3 V/5 V level shifting where required.

## Build and upload

Build and upload with the Arduino IDE or CLI:

```powershell
arduino-cli compile --upload -p COM22 --fqbn arduino:avr:mega src\vera_driver
```

Replace `COM22` with the Mega's port. The bridge is now a binary protocol
endpoint at 500,000 baud; use the Qt GUI rather than a terminal.

## Bridge protocol

Frames use an XOR checksum:

```text
request:  A5 opcode length-lo length-hi payload checksum
response: 5A status opcode length-lo length-hi payload checksum
```

Supported commands are `PING`, `RESET`, `WRITE_REGISTER`, `READ_REGISTER`,
`WRITE_VRAM`, `READ_VRAM`, and `SPI_TRANSFER`. Each VRAM-write or SPI-transfer
request transfers up to 1,024 bytes. SPI transfers poll VERA locally on the
Mega, so SD-sector reads do not incur one USB round trip per byte. The protocol
is documented in the sketch header.

## Qt 6 GUI

The GUI contains the VERA-facing policy formerly embedded in the sketch:

- serial-port discovery and bridge handshake;
- FPGA version recovery via DCSEL banks 60--63;
- VGA display configuration;
- palette conversion and chunked VRAM upload;
- the pixel-exact 320×240 indexed PM5544 card, rendered by VERA as an 8bpp
  bitmap and scaled to 640×480.
- dedicated 640×480 1bpp monochrome, 2bpp four-colour, bitmap-text, and
  4bpp tiled-rendering tests.
- 16×16 4bpp sprite and continuous 440 Hz PSG-audio tests.
- VSync/programmable-line interrupt statistics and a read-only SD FAT32
  diagnostic: SPI startup (CMD0, CMD8, ACMD41, CMD58), MBR, first FAT32 boot
  sector, and FSInfo validation.

From an MSYS2 MinGW shell with Qt 6 installed:

```sh
cd driver/gui
cmake -S . -B build -G Ninja
cmake --build build
./build/vera-gui.exe
```

The GUI embeds the indexed PM5544 PNG as a Qt resource. Its SVG source,
reference rasters, generator, and attribution remain in `driver/assets/` and
`driver/tools/`.
