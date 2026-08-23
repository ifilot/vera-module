# VERA Raspberry Pi Pico programmer

This directory contains everything needed to build and use the VERA programmer:

- `firmware`: Raspberry Pi Pico firmware and its USB protocol
- `cli`: native Windows command-line flashing and verification tool
- `pcb`: KiCad schematic and PCB layout for the programmer hardware

See each subdirectory's README for build and usage instructions.

The PCB and firmware pin assignments are kept together in the firmware's
`programmer_config.h`. The board's dedicated READ (GPIO 26) and WRITE (GPIO 27)
LEDs show flash activity, while the Pico onboard LED (GPIO 25) shows whether the
FPGA completed booting.

The programmer firmware and CLI use the ecosystem version from the repository
root `VERSION` file.
