Programming Interface
=====================

Register window
---------------

The host accesses VERA through this register window. ``ADDR0`` and ``ADDR1``
are independent VRAM pointers; ``CTRL.ADDRSEL`` selects which pointer is being
configured.

.. list-table::
   :header-rows: 1

   * - Address
     - Name
     - Purpose
   * - $9F20
     - ADDRx_L
     - Bits 7:0 of the selected VRAM address
   * - $9F21
     - ADDRx_M
     - Bits 15:8 of the selected VRAM address
   * - $9F22
     - ADDRx_H
     - Address bit 16 and increment configuration
   * - $9F23
     - DATA0
     - Read or write through VRAM pointer 0
   * - $9F24
     - DATA1
     - Read or write through VRAM pointer 1
   * - $9F25
     - CTRL
     - Reset, display-controller bank, and address selection
   * - $9F26
     - IEN
     - Interrupt enables
   * - $9F27
     - ISR
     - Interrupt status; write one bits to acknowledge
   * - $9F28
     - IRQLINE
     - Requested raster line on write; current scanline on read

VRAM access
-----------

Set ``ADDRx_L``, ``ADDRx_M``, and ``ADDRx_H`` first, then transfer bytes via
the matching data port. The high address register contains the 17th address bit
and a four-bit increment code. After every access to a data port, its pointer
advances by the selected amount.

==============  =================
Increment code  Address increment
==============  =================
0               0
1 to 5          1, 2, 4, 8, 16
6 to 10         32, 64, 128, 256, 512
11 to 15        1024 through 16384
==============  =================

Use an increment of one for sequential uploads. The two pointers are especially
handy when copying or interleaving data: one can remain pointed at source data
while the other addresses a destination or a register-backed stream.

Memory map
----------

The 128 KiB internal address space is arranged as follows:

=====================  =================================
Range                  Contents
=====================  =================================
``$0:0000-$1:F9BF``    General video RAM
``$1:F9C0-$1:F9FF``    PSG registers
``$1:FA00-$1:FBFF``    256-entry palette
``$1:FC00-$1:FFFF``    128 sprite attribute records
=====================  =================================

Interrupts
----------

``IEN`` enables vertical-sync, raster-line, sprite-collision, and audio-FIFO
interrupts. ``ISR`` reports latched events. Acknowledge an event by writing a
one to its status bit; writing zero leaves it unchanged. Set ``IRQLINE`` before
enabling the line interrupt. Poll the same address to obtain the current
scanline.

.. _version-registers:

Version registers
-----------------

The display-controller register bank is selected through ``CTRL.DCSEL``.
With bank 63 selected, addresses ``$9F29`` through ``$9F2C`` return the module
signature and version. The first byte is ASCII ``V`` (``$56``); the next three
bytes provide the major, minor, and build values. Treat a different first byte
as an unknown implementation.
