The VERA Ecosystem
==================

The repository contains more than the module PCB. Its tools form a path from
source code to a programmed board and then to an interactive hardware test.

FPGA firmware
-------------

The Verilog sources live in ``firmware/source``. They target the
iCE40UP5K-SG48 and build with Yosys, nextpnr, and Project IceStorm. The pinned
container build produces ``firmware/build/vera.bin`` and performs timing
checks before accepting the image.

Programmer and command-line tool
--------------------------------

The dedicated programmer is based on a Raspberry Pi Pico. Its PCB, firmware,
and native command-line application are kept together under
``toolchain/programmer``. The command-line tool identifies the flash device,
programs the FPGA image, verifies its contents, and reports failures rather
than leaving a partly programmed device looking successful.

The programmer's READ and WRITE LEDs show flash activity. The Pico's onboard
LED reports whether the FPGA completed its boot sequence. This makes the tool
useful both for manufacturing and for firmware development.

Arduino Mega bridge
-------------------

The driver PCB and firmware turn an Arduino Mega 2560 into a USB-to-parallel
bridge. The bridge owns the electrical bus cycles and exposes a framed serial
protocol at 500,000 baud. Commands cover reset, register access, block VRAM
transfers, and SPI transfers.

Keeping graphics policy on the computer and bus timing on the bridge has two
benefits: the firmware stays small, and large test assets can be prepared by
the desktop application before being sent to VERA.

VERA Test Console
-----------------

The Qt desktop application provides a guided hardware test surface. It can
discover and connect to the bridge, identify the FPGA image, configure VGA,
upload test patterns, exercise tile and sprite rendering, play PSG audio,
inspect interrupts, and perform a read-only SD-card diagnostic.

Continuous integration builds a self-contained Windows installer with the Qt
runtime included. An end user therefore needs neither a compiler nor a
separate Qt installation. See :doc:`gui` for the practical workflow.
