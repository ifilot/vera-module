VERA Test Console
=================

The VERA Test Console is a Windows desktop application for testing a module
through the Arduino Mega bridge. It exercises real hardware through the same
32 register offsets described in this manual.

Install and connect
-------------------

#. Download the Windows installer artifact produced by the **GUI installer**
   workflow.
#. Run ``vera-test-console-<version>-windows-x86_64-setup.exe``.
#. Connect the Arduino Mega bridge to the module and the computer.
#. Start **VERA Test Console** and select the bridge's serial port.
#. Choose **Connect** and confirm that the firmware identifier appears.

The installer contains the required Qt runtime. The target computer does not
need Qt, CMake, or a development environment.

Test progression
----------------

Begin with **VGA signal only**. This verifies configuration and output timing
without depending on a VRAM test pattern. Then work through the bitmap modes,
tile rendering, sprites, audio, interrupts, and SD-card diagnostic. The audio
section includes a left/right stereo test and looping Game Boy-style Tetris and
NES-style Super Mario Bros. Ground Level PSG themes. Running the tests in this
order makes a wiring or bus problem easier to isolate.

Large assets are converted on the computer and transferred in bounded VRAM
blocks. The console reports progress rather than appearing unresponsive during
an upload. Its storage diagnostic is read-only: it initializes the card over
SPI and inspects its partition and filesystem metadata without modifying it.

Troubleshooting
---------------

**No serial ports shown**
   Confirm that the operating system recognizes the Mega and that no other
   application has its port open.

**Handshake fails**
   Check that the bridge firmware is installed, the selected port is correct,
   and the bridge and module share ground. Verify the address, data, and three
   bus-control connections against :doc:`integration`.

**Identifier is empty or corrupted**
   Check ``D0-D7`` for swapped lines and ``A0-A4`` for incorrect ordering. A
   stable but wrong value often indicates an address-selection problem.

**VGA test works but graphics do not**
   The configuration and display clock are alive. Concentrate on write cycles,
   VRAM pointer setup, and data-bus ordering.

**SD-card test fails**
   Reseat the card, start at the slow SPI clock, and check the appropriate card
   socket for the selected board configuration.

Bridge wiring
-------------

The supplied firmware uses Mega pins 22-29 for ``D0-D7``, 30-34 for ``A0-A4``,
35 for ``/CS``, 36 for ``/RD``, 37 for ``/WR``, and 38 for ``/RESET``. Use the
driver PCB or suitable level shifting; do not connect voltage domains based on
pin numbers alone.
