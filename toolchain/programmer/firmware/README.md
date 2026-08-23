# VERA programmer firmware

This directory contains Raspberry Pi Pico firmware for programming the VERA
module's W25Q16 SPI flash and controlling the iCE40 FPGA reset and boot pins.
The Pico exposes an eight-character command protocol over USB CDC.

## Build

The build downloads the pinned Raspberry Pi Pico SDK automatically. It requires
CMake, Git, and an `arm-none-eabi-gcc` compiler. Set `PICO_SDK_PATH` only when
you want to use an existing SDK checkout instead:

```sh
cmake -S toolchain/programmer/firmware -B build/programmer-firmware
cmake --build build/programmer-firmware
```

Select the Raspberry Pi Pico 2 instead of the original Pico with:

```sh
cmake -S toolchain/programmer/firmware \
  -B build/programmer-firmware-pico2 \
  -DPICO_BOARD=pico2
cmake --build build/programmer-firmware-pico2
```

The UF2 image is written to
`build/programmer-firmware/vera-programmer.uf2`. Hold the Pico's BOOTSEL button
while connecting it, then copy that file to the mounted `RPI-RP2` drive.

## Pico pin mapping

| Signal | GPIO |
| --- | ---: |
| FPGA CDONE | 14 |
| FPGA CRESET | 15 |
| SPI MISO | 16 |
| SPI chip select | 17 |
| SPI clock | 18 |
| SPI MOSI | 19 |
| Pico boot-status LED | 25 |
| PCB READ LED (D2) | 26 |
| PCB WRITE LED (D3) | 27 |

The PCB LEDs are active-high. READ is illuminated while the programmer is
reading or verifying flash data; WRITE is illuminated while it is receiving or
modifying flash data. The Pico's onboard LED reflects the last FPGA boot
result: it is off while the FPGA is held in reset or when `CDONE` stays low,
and on after a successful boot.

## USB commands

Every request is exactly eight uppercase alphanumeric characters. The firmware
first echoes the request and then writes any command-specific response.

| Command | Operation | Response after echo |
| --- | --- | --- |
| `READINFO` | Read programmer identity | `VERA-PROG-v0.1.0` |
| `DEVIDSST` | Read JEDEC ID | Three bytes |
| `ERASBKxx` | Erase 64 KiB block `xx` | Block byte |
| `RESETCHP` | Reset the SPI flash | None |
| `CHCKBKxx` | Check whether block `xx` is erased | `00` if erased, `FF` otherwise |
| `RDPGxxxx` | Read 256-byte page `xxxx` | 256 bytes |
| `WRSECTxx` | Write 4096-byte sector `xx` | CRC-16/XMODEM, little endian |
| `BOOTFPGA` | Release FPGA reset | One-byte CDONE state |
| `HOLDFPGA` | Hold FPGA in reset | `00` |

Numeric command suffixes are hexadecimal.

The firmware rejects non-hexadecimal numeric suffixes instead of interpreting
them as address zero. It also bounds sector-data reception and flash busy waits,
and reports erase/program failures through the existing command responses so
the protocol remains compatible with the CLI.

The identity version comes from the repository's root `VERSION` file. The
initial ecosystem release is `v0.1.0`; changing `VERSION` updates both the Pico
firmware identity and the matching host CLI expectation.
