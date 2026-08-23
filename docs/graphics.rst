Graphics Programming
====================

The display composer combines two identical graphics layers and a sprite
plane. All pixel data is palette indexed. Palette index zero is transparent in
layers and sprites; the border may still use entry zero as an ordinary color.

Composition order
-----------------

From front to back, the output is composed as follows:

#. Sprites with Z-depth 3
#. Layer 1
#. Sprites with Z-depth 2
#. Layer 0
#. Sprites with Z-depth 1
#. Palette entry 0

A disabled layer or a pixel with index zero reveals the content below it.

Display setup
-------------

Use ``DCSEL=1`` to define the active rectangle in the native 640 x 480 space.
Horizontal boundaries have four-pixel precision and vertical boundaries have
two-line precision. Then use ``DCSEL=0`` to choose the output, scaling, border,
and enabled renderers.

``DC_HSCALE`` and ``DC_VSCALE`` are fractional source steps. A value of 128
maps one source pixel to one output pixel. A value of 64 repeats each source
pixel twice, which makes a 320 x 240 source fill a 640 x 480 active area.

Palette format
--------------

The 256 RGB444 entries begin at VRAM ``0x1FA00``. Entry *n* occupies
``0x1FA00 + 2*n``:

.. list-table:: Palette entry
   :header-rows: 1
   :widths: 18 30 52

   * - Byte
     - Bits
     - Value
   * - 0
     - ``[7:4] G``, ``[3:0] B``
     - Four-bit green and blue components
   * - 1
     - ``[7:4]`` unused, ``[3:0] R``
     - Four-bit red component

For example, bright red is stored as ``0x00, 0x0F`` and bright cyan as
``0xFF, 0x00``. Reset loads a default palette, but portable applications should
write every color they use.

Layer modes
-----------

Both layers support tile and bitmap modes at 1, 2, 4, or 8 bits per pixel.
Packed pixels are ordered most-significant first: in 4 bpp, the high nibble is
the left pixel; in 2 bpp, bits 7:6 are the leftmost pixel.

Tile mode
^^^^^^^^^

``Lx_MAPBASE`` points to a map of two-byte entries. ``Lx_TILEBASE`` points to
the pixel patterns. Maps can be 32, 64, 128, or 256 tiles in each dimension;
tiles can independently be 8 or 16 pixels wide and high.

In 2, 4, and 8 bpp modes, a map entry is:

.. list-table:: General tile-map entry
   :header-rows: 1
   :widths: 15 85

   * - Byte
     - Fields
   * - 0
     - Tile index bits 7:0
   * - 1
     - ``[7:4]`` palette offset, ``[3]`` vertical flip, ``[2]`` horizontal
       flip, ``[1:0]`` tile index bits 9:8

Index zero stays transparent. Indexes 1-15 receive ``16 * palette_offset``;
indexes 16-255 are unchanged. In these modes, setting ``ATTR`` also forces bit
7 of the resulting color index.

One-bit tile mode has two useful forms:

* With ``ATTR=0``, map byte 0 is the character index. The high and low nibbles
  of byte 1 select 16-color background and foreground indexes.
* With ``ATTR=1``, map byte 0 is the character index and byte 1 is an 8-bit
  foreground index. Clear pixels use transparent index zero.

Horizontal and vertical scroll values range from 0 through 4095. Increasing a
scroll value moves the visible content left or up. The map wraps at its
configured dimensions.

Bitmap mode
^^^^^^^^^^^

In bitmap mode ``Lx_MAPBASE`` is unused and ``Lx_TILEBASE`` points to the pixel
array. ``TILEW=0`` selects a 320-pixel row; ``TILEW=1`` selects 640 pixels.
Hardware scrolling is unavailable.

The low horizontal-scroll register and both vertical-scroll registers are
ignored. The low nibble of ``Lx_HSCROLL_H`` becomes a bitmap palette offset.
Color remapping follows the tile-mode rule: indexes 1-15 receive the offset,
while zero and indexes 16-255 remain unchanged. ``ATTR`` forces color-index bit
7 in modes above 1 bpp.

Sprites
-------

VERA has 128 sprites. Their eight-byte records occupy VRAM
``0x1FC00-0x1FFFF``. Sprite image data belongs in general VRAM and must begin at
a 32-byte boundary.

.. list-table:: Sprite record
   :header-rows: 1
   :widths: 12 88

   * - Byte
     - Fields
   * - 0
     - Image address bits 12:5
   * - 1
     - ``[7]`` mode (0 = 4 bpp, 1 = 8 bpp), ``[6:4]`` reserved,
       ``[3:0]`` image address bits 16:13
   * - 2
     - X position bits 7:0
   * - 3
     - ``[7:2]`` reserved, ``[1:0]`` X position bits 9:8
   * - 4
     - Y position bits 7:0
   * - 5
     - ``[7:2]`` reserved, ``[1:0]`` Y position bits 9:8
   * - 6
     - ``[7:4]`` collision mask, ``[3:2]`` Z-depth, ``[1]`` vertical flip,
       ``[0]`` horizontal flip
   * - 7
     - ``[7:6]`` height, ``[5:4]`` width, ``[3:0]`` palette offset

Width and height codes ``0-3`` mean 8, 16, 32, and 64 pixels. Z-depth ``0``
disables the sprite; depths ``1``, ``2``, and ``3`` place it at the positions
shown in the composition order. Positions are 10-bit values, allowing a
sprite to move cleanly through the edges of the visible area.

Sprites are considered in ascending record order. At equal Z-depth, a lower
record number has priority. In 4 bpp mode, the palette offset selects one of 16
palette blocks; 8 bpp pixels use their complete index.

Collision groups
^^^^^^^^^^^^^^^^

Each sprite has a four-bit collision mask. When opaque pixels from two sprites
overlap, the renderer ANDs their masks. A nonzero result contributes to the
collision field and raises the collision event. At vertical blank, ``ISR[7:4]``
reports the accumulated groups and ``ISR.SPRCOL`` reports the latched event.

Only rendered pixels participate. Disabled sprites, transparent pixels, and
content outside rendered lines do not collide.

A dependable first image
------------------------

#. Disable video output and both layers.
#. Write the needed palette entries at ``0x1FA00``.
#. Upload a 320 x 240 8 bpp bitmap to a 2048-byte-aligned VRAM address.
#. Set layer 0 to bitmap mode, 8 bpp, 320-pixel width, and that base address.
#. Set both scale registers to 64 and select the full 640 x 480 active area.
#. Select VGA output and enable layer 0.

This path avoids tile-map setup and makes bus, VRAM, palette, and video errors
easy to distinguish during initial integration.
