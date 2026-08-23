# VERA Arduino Mega Driver

This folder contains an Arduino Mega sketch for smoke-testing a CX16 VERA board
over an 8-bit parallel bus. After reset it performs a VRAM readback check, then
runs an autonomous diagnostics loop. No keyboard is required.

## Layout

- `vera_driver/vera_driver.ino`: Arduino sketch.
- `vera_driver/p2000c_font.h`: Generated 8x12 font data stored in Arduino flash.

## Expected result

After upload, open the serial monitor at `115200` baud. Startup prints the VERA
reset and VRAM readback result, then each diagnostics scene announces itself:

```text
PASS: VERA bus and VRAM access work
Scene: boot summary
Scene: 4bpp bitmap color bars
Scene: glyph grid
Scene: sprite walk
Scene: layer 1 tile scroll
Scene: PSG audio
Starting VERA PSG stereo sound check...
Sound check: left one beep, right two beeps
Scene: SD sector read
Starting VERA SD card sector 0 check...
SD check: sector 0 read and displayed
Starting VERA sprite walk test...
```

On the connected VERA display, the diagnostics loop cycles through:

- Boot summary with VRAM readback and visible edge markers.
- 4bpp 320x240 bitmap color bars and palette pattern.
- Full-screen glyph grid using the P2000C-derived 8x12 font.
- Generated 16x16 4bpp sprite walking over a 1bpp bitmap background.
- Transparent layer 1 tilemap scrolling soft 4x4 checker tiles over a static
  grid on layer 0.
- PSG stereo audio check: one left beep, two right beeps.
- Read-only SD card check: initialize card, read sector 0, and display a
  512-byte hex plus printable ASCII dump with the sprite walking over it.

The SD check is read-only. If a card does not respond, the display and serial
monitor show the failing stage, such as `CMD0`, `CMD8`, `ACMD41`, `CMD58`, or
`CMD17`.

The sprite is generated in code, so no external asset file is needed.

The Arduino Mega data bus uses direct `PORTA` access on pins 22..29. VERA
address and control lines use direct `PORTC` access on pins 30..37, with only a
few AVR `nop` instructions between `/CS`, `/RD`, and `/WR` transitions. This is
much faster than the original conservative `digitalWrite()` plus microsecond
delay bus cycle.

If your board is wired for composite/S-video instead, change this line in
`vera_driver.ino`:

```cpp
const uint8_t display_output_mode = 1;
```

to:

```cpp
const uint8_t display_output_mode = 2;
```

## Build on Linux

Install Arduino CLI, install the AVR core, then compile from the repository
root:

```sh
arduino-cli core update-index
arduino-cli core install arduino:avr
arduino-cli compile --output-dir src/build --fqbn arduino:avr:mega src/vera_driver
```

Or use the wrapper from this folder:

```sh
cd src
make install-avr
make
```

For an Arduino Mega 2560 connected directly to Linux, upload with:

```sh
arduino-cli upload -p /dev/ttyACM0 --fqbn arduino:avr:mega src/vera_driver
```

Replace `/dev/ttyACM0` with the port shown by:

```sh
arduino-cli board list
```

## Flash from Windows

The simplest path is Arduino IDE or Arduino CLI on Windows. To compile and
upload in one step:

```powershell
arduino-cli core update-index
arduino-cli core install arduino:avr
arduino-cli board list
arduino-cli compile --upload -p COM22 --fqbn arduino:avr:mega src\vera_driver
```

Replace `COM22` with the port shown by Device Manager or `arduino-cli board
list`.

If building in Linux or WSL and flashing from Windows, compile with
`--output-dir src/build`, then flash the generated
`src/build/vera_driver.ino.hex` from Windows with Arduino CLI, Arduino IDE, or
`avrdude` from the Arduino AVR tools package.
