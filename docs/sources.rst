Sources and Scope
=================

This manual describes the standalone VERA module implemented in this
repository. It uses register offsets and deliberately leaves host address
decoding to the integrator.

Primary material
----------------

* Frank van den Hoef's `VERA Module rev4 hardware manual
  <https://github.com/fvdhoef/vera-module/blob/rev4/doc/vera-module.pdf>`_
  is the canonical source for the original board, external interface, and
  design intent. Its name expansion differs from both the original repository
  title and the later programmer's reference; :doc:`story` records all three.
* A later VERA Programmer's Reference was used for the graphics, audio, SPI,
  and VRAM descriptions. Host-specific addresses, software conventions, and
  features absent from this FPGA core were intentionally excluded.
* The maintained ``firmware/source/top.v`` is the source of truth for the
  offset map, implemented display-controller banks, reserved bits, interrupt
  behavior, and pointer increments documented here.
* The KiCad files under ``pcb/vera-full-audiojack`` and
  ``pcb/vera-mini-audiojack`` are the source of truth for electrical and
  connector details.

Historical cross-checks
-----------------------

The origin account was checked against the preserved repository history and
the independent `Understanding VERA
<https://epsilon537.github.io/boxlambda/understanding-vera/>`_ technical
overview. The upstream history ends on 20 October 2023; the statement that no
further upstream development occurred in 2024 follows from that public Git
record. The 2025 continuation date records the maintained project's own
timeline.

Documentation policy
--------------------

The HDL wins when a secondary programmer's guide describes behavior not
present in this core. Reserved fields are documented as reserved, and
unimplemented display-controller banks are not presented as available
features. This keeps the manual useful for both existing software and new host
designs without promising hardware that is not in the bitstream.
