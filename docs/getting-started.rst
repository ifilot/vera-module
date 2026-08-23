Getting Started
===============

What VERA provides
------------------

VERA generates a 640 by 480, 60 Hz video signal and can drive VGA, composite,
and RGB outputs. Its renderer combines two independent layers with up to 128
sprites. Each layer can be a tiled surface or a bitmap. Colour indexes are
looked up in a 256-entry RGB444 palette.

Alongside video, the module includes a 16-channel programmable sound generator
(PSG), a PCM FIFO, and an SPI controller connected to the SD-card socket.

Bring-up checklist
------------------

#. Ensure the module has a stable supply and a correctly configured FPGA image.
#. Connect the host or bridge to the 8-bit data bus, five register-address
   lines, and the active-low ``/CS``, ``/RD``, ``/WR``, and ``/RESET`` signals.
#. Attach the desired display output and, where needed, audio and SD card.
#. Reset the module, then read the four version registers as described in
   :ref:`version-registers`.
#. Select a display mode, initialise a palette, fill a small VRAM region, and
   enable a layer. Start with a 320 by 240 bitmap: it is economical in VRAM and
   is scaled cleanly to the native output.

The included VERA Test Console is a convenient first validation step when using
the supplied USB-to-parallel bridge. Its connection and diagnostic workflow is
described in :doc:`gui`.

Signals and electrical integration
----------------------------------

The module presents an 8-bit, register-addressed parallel interface. The host
sets the register address on A0 through A4, places or samples a byte on D0
through D7, and performs a read or write while the chip-select signal is
active. Keep the data bus driven only for writes; the module drives it for
reads.

``/RESET`` resets the FPGA configuration and returns registers to their reset
state. The reset palette is populated with useful defaults, but software should
always program the entries it depends on.

Video modes
-----------

.. list-table::
   :header-rows: 1

   * - Mode
     - Value
   * - Video disabled
     - 0
   * - VGA
     - 1
   * - NTSC composite
     - 2
   * - RGB, composite sync
     - 3

The visible region is expressed in the native 640 by 480 coordinate system.
For a full-frame picture, use HSTART 0, HSTOP 640, VSTART 0, and VSTOP 480.
