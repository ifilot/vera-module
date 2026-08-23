VERA Module
===========

**VERA** is the Versatile Embedded Retro Adapter: a self-contained FPGA module
for video, audio, and SD-card access. A host controls it through a compact
8-bit parallel register interface. The host architecture, processor, and
address-decoding scheme are deliberately left to the system designer.

VERA combines two tile-or-bitmap graphics layers, 128 sprites, a 256-entry
RGB444 palette, a 16-voice sound generator, PCM playback, and an SPI
controller. The maintained project adds reproducible open-source FPGA builds,
dedicated programming tools, a USB bridge, and a desktop test console while
keeping compatibility with the original module central to the design.

Start here
----------

* :doc:`story` introduces the design and the maintained project.
* :doc:`hardware` compares the two supported module configurations.
* :doc:`integration` explains the bus pins and how to attach VERA to a host.
* :doc:`registers` is the authoritative offset-based programming reference.
* :doc:`vram` and :doc:`graphics` cover memory layout and rendering.

.. note::

   Register numbers in this manual are **offsets selected by A0-A4**, not CPU
   memory addresses. Your hardware may expose them through memory-mapped I/O,
   an isolated I/O space, a microcontroller GPIO bus, or another bridge.

.. toctree::
   :maxdepth: 2
   :caption: Overview

   story
   ecosystem
   hardware

.. toctree::
   :maxdepth: 2
   :caption: Integration

   integration
   registers
   vram

.. toctree::
   :maxdepth: 2
   :caption: Programming

   graphics
   audio-storage
   gui

.. toctree::
   :maxdepth: 1
   :caption: Project

   sources
