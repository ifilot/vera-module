Connecting a Host
=================

VERA is a peripheral, not a complete host bus. The system designer supplies
the address decoding and decides where the 32 registers appear.

Parallel interface
------------------

.. list-table:: Host-facing signals
   :header-rows: 1
   :widths: 18 16 66

   * - Signal
     - Direction
     - Function
   * - ``A0-A4``
     - Host to VERA
     - Select one of 32 register offsets, ``0x00`` through ``0x1F``.
   * - ``D0-D7``
     - Bidirectional
     - Carry one byte. The host drives writes; VERA drives reads.
   * - ``/CS``
     - Host to VERA
     - Active-low chip select for a register transaction.
   * - ``/RD``
     - Host to VERA
     - Active-low read strobe.
   * - ``/WR``
     - Host to VERA
     - Active-low write strobe.
   * - ``/IRQ``
     - VERA to host
     - Active-low interrupt request. It remains asserted while an enabled
       interrupt source is pending.
   * - ``/RESET``
     - Host to module
     - Active-low FPGA configuration reset exposed by the board.

The board includes level translation on the parallel interface. Confirm the
carrier's voltage and drive requirements against the current board schematic;
do not infer them from the FPGA core voltage.

Read and write cycles
---------------------

For a write, place the offset on ``A0-A4`` and the byte on ``D0-D7``, assert
``/CS``, then pulse ``/WR`` low while ``/RD`` remains high. Release the write
strobe before removing the address and data.

For a read, place the offset on ``A0-A4``, leave the host data drivers in their
high-impedance state, assert ``/CS``, and pulse ``/RD`` low while ``/WR``
remains high. Sample ``D0-D7`` while the read is valid. VERA releases the data
bus outside an active read.

.. warning::

   Never let the host and VERA drive ``D0-D7`` at the same time. The host must
   turn its output drivers off before beginning a read cycle.

Memory-mapped or port-mapped
----------------------------

Both integration styles are valid:

* A memory decoder may assert ``/CS`` for a chosen 32-byte region and connect
  CPU address bits 0-4 to VERA ``A0-A4``.
* An isolated I/O decoder may expose the same offsets through the processor's
  port instructions.
* A microcontroller may generate complete transactions with GPIO, as the
  included Arduino Mega bridge does.

Host-side code is easiest to reuse when it operates on register offsets:

.. code-block:: c

   enum vera_register {
       VERA_ADDR_LOW  = 0x00,
       VERA_ADDR_MID  = 0x01,
       VERA_ADDR_HIGH = 0x02,
       VERA_DATA0     = 0x03,
       VERA_CONTROL   = 0x05
   };

   void vera_write(uint8_t offset, uint8_t value);
   uint8_t vera_read(uint8_t offset);

Only the two functions above need to know whether the connection is a memory
window, an I/O port, or a bridge protocol.

Interrupt handling
------------------

``/IRQ`` is active low. Enable sources in :ref:`reg-ien`, then inspect
:ref:`reg-isr` when an interrupt arrives. Vertical sync, line, and sprite
collision events are latched and cleared by writing a one to the corresponding
status bit. Audio FIFO low is a live condition and clears after the FIFO has
been refilled sufficiently.

Bring-up sequence
-----------------

#. Check power, ground, connector orientation, and level translation.
#. Pulse ``/RESET`` low and allow the FPGA to configure.
#. Read the :ref:`firmware-id` to verify communication.
#. Keep video disabled while initializing VRAM and registers.
#. Configure the active area, scale, palette, and one graphics layer.
#. Select the board's available output and enable the layer.
#. Add interrupts, audio, sprites, and storage one subsystem at a time.
