Reference Notes
===============

This documentation is a practical guide, not a replacement for the detailed
hardware specification. It is based on the original VERA module documentation
and the VERA Programmer's Reference. Check the installed FPGA version before
depending on a feature, especially when working with experimental display
controller functions.

Operational reminders
---------------------

* Reset restores registers and the default palette; initialise all state your
  program needs.
* Keep VRAM allocations clear of the fixed PSG, palette, and sprite-attribute
  ranges.
* Use vertical sync or a line interrupt for visible updates that must not tear.
* Index 0 is transparent in indexed tile and sprite data.
* Configure display output before enabling layers or sprites.
* Check SPI busy state around every byte transfer.

For repository-specific build and programming information, see the firmware,
programmer, and bridge documentation alongside their source directories.
