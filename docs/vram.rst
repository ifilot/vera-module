Working with VRAM
=================

VERA contains 128 KiB of internal memory addressed from ``0x00000`` through
``0x1FFFF``. It is not placed directly on the host bus. Software reaches it
through the two address pointers and data ports described in
:doc:`registers`.

Address-space layout
--------------------

.. list-table:: VRAM regions
   :header-rows: 1
   :widths: 34 30 36

   * - Range
     - Size
     - Purpose
   * - ``0x00000-0x1F9BF``
     - 127,424 bytes
     - General video data: bitmaps, maps, tiles, and sprite pixels
   * - ``0x1F9C0-0x1F9FF``
     - 64 bytes
     - 16 PSG voices, four bytes each
   * - ``0x1FA00-0x1FBFF``
     - 512 bytes
     - 256 palette entries, two bytes each
   * - ``0x1FC00-0x1FFFF``
     - 1,024 bytes
     - 128 sprite records, eight bytes each

The general region has no prescribed software layout. An application may put
its bitmap, maps, tiles, and sprite images anywhere they fit while respecting
the alignment required by the relevant base registers. The three upper
regions have hardware-defined meanings.

Setting a pointer
-----------------

To configure pointer 0 or 1:

#. Read ``CTRL`` and set ``ADDRSEL`` to the desired pointer without changing
   ``DCSEL``.
#. Write address bits 7:0 to ``ADDRx_L``.
#. Write address bits 15:8 to ``ADDRx_M``.
#. Write the increment code, direction, and address bit 16 to ``ADDRx_H``.
#. Access ``DATA0`` for pointer 0 or ``DATA1`` for pointer 1.

This helper configures a pointer while preserving the display-bank selection:

.. code-block:: c

   void vera_set_vram_address(unsigned port, uint32_t address,
                              uint8_t increment, bool decrement)
   {
       uint8_t control = vera_read(0x05);
       control = (control & 0xfe) | (port & 1);
       vera_write(0x05, control);
       vera_write(0x00, address & 0xff);
       vera_write(0x01, (address >> 8) & 0xff);
       vera_write(0x02, (increment << 4) |
                        (decrement ? 0x08 : 0x00) |
                        ((address >> 16) & 1));
   }

Sequential transfers
--------------------

Increment code 1 advances by one byte after every ``DATA0`` or ``DATA1``
access. It is the normal choice for uploads and downloads:

.. code-block:: c

   vera_set_vram_address(0, destination, 1, false);
   for (size_t i = 0; i < length; ++i)
       vera_write(0x03, source[i]);

Address writes initiate the read prefetch used by the corresponding data port.
After setting all three address bytes, the first data-port read returns the
byte at that address. Each read returns the prefetched byte, advances the
pointer, and fetches the next one. Avoid changing a pointer between setting it
and consuming its first byte.

Two-port patterns
-----------------

The two pointers are independent. Common uses include:

* keeping ``DATA0`` on a sequential source while ``DATA1`` updates a palette
  or attribute table;
* using a row-sized increment to update one column of a bitmap or tile map;
* maintaining separate producer and inspection positions in a debugger.

``ADDRSEL`` affects only which pointer the three address registers configure.
It does not redirect the data ports.

Alignment and allocation
------------------------

Tile-map bases must be aligned to 512 bytes, and tile or bitmap bases must be
aligned to 2048 bytes. Sprite image addresses are stored in units of 32 bytes.
Plan allocations from the bottom of general VRAM upward and keep them below
``0x1F9C0``.

The PSG, palette, and sprite ranges are backed by write-oriented hardware
tables. Initialize every entry that the application depends on rather than
assuming that reading one of these regions always reveals the active internal
state.
