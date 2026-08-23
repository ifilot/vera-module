Hardware Configurations
=======================

Two module configurations are maintained. They use the same FPGA firmware and
the same host register interface, so software does not need a board-specific
driver.

.. list-table:: Supported boards
   :header-rows: 1
   :widths: 28 36 36

   * - Feature
     - ``vera-full-audiojack``
     - ``vera-mini-audiojack``
   * - Form factor
     - Full-size module
     - Reduced-size module
   * - Digital video
     - VGA connector
     - VGA connector
   * - Analog video
     - Composite and S-video connectors
     - Not fitted
   * - Audio
     - Stereo audio jack
     - Stereo audio jack
   * - Removable storage
     - MicroSD-card socket
     - MicroSD-card socket
   * - Host interface
     - 8-bit parallel bus
     - 8-bit parallel bus

Full audio-jack board
---------------------

Choose ``vera-full-audiojack`` when the analog video outputs are useful. It
provides VGA, composite video, S-video, stereo audio, and a microSD-card socket
on one board. The additional connectors and analog output circuitry require
the larger outline.

Mini audio-jack board
---------------------

Choose ``vera-mini-audiojack`` when board area matters and VGA is the required
display connection. It keeps the stereo audio jack and microSD-card socket in
a smaller outline. The parallel bus and programming connection remain
compatible with the full board.

Common architecture
-------------------

Both boards contain an iCE40UP5K FPGA, 128 KiB of internal video memory,
configuration flash, a 25 MHz oscillator, level translation for the host bus,
and a WM8524 audio DAC. Both expose the same five address signals, eight data
signals, active-low bus controls, and active-low interrupt output.

Use the KiCad schematic for the selected board as the electrical source of
truth. In particular, check connector numbering, supply pins, signal levels,
and programming-header orientation before designing a carrier board.
