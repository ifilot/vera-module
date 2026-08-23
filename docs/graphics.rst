Graphics
========

Display composer
----------------

With display-controller bank 0 selected, ``DC_VIDEO`` chooses the output mode
and enables layer 0, layer 1, and sprites. ``DC_HSCALE`` and ``DC_VSCALE``
control fractional scaling of the active display: 128 maps one source pixel to
one output pixel, while 64 doubles each source pixel. ``DC_BORDER`` holds the
palette index used outside the active region.

Layers
------

The two layers have identical control registers. Each can operate in a tile or
bitmap mode, at 1, 2, 4, or 8 bits per pixel where supported. In tile mode,
the map base is aligned to 512 bytes and the tile base to 2048 bytes. Tile maps
can be 32, 64, 128, or 256 tiles wide and high; tiles are 8 or 16 pixels wide
and high.

In bitmap mode the map base is unused. The tile-base register names the bitmap
address and the tile-width bit selects a 320- or 640-pixel source width. A
bitmap palette offset can remap indexed colours 1 through 15 in blocks of 16;
index 0 remains transparent.

Tile entries are two bytes. For 2-, 4-, and 8-bit tiles they identify the tile,
choose a palette offset, and optionally flip the tile horizontally or
vertically. The renderer treats index 0 as transparent.

Palette
-------

The palette starts at ``$1:FA00``. Each of its 256 entries consumes two bytes:
the first stores blue in bits 3:0 and green in bits 7:4; the second stores red
in bits 3:0. This RGB444 arrangement gives 4,096 possible output colours.

Sprites
-------

VERA provides 128 attribute records, eight bytes each, beginning at
``$1:FC00``. A record supplies the pixel-data address, 10-bit X and Y position,
colour mode, palette offset, flip flags, collision group, Z-depth, and a size
from 8 by 8 to 64 by 64 pixels.

Z-depth determines where a sprite appears: disabled, behind layer 0, between
the layers, or in front of layer 1. Use the sprite-collision interrupt to learn
when rendered sprite groups overlap; it is evaluated during vertical blank.

Suggested first frame
---------------------

#. Upload two palette entries to ``$1:FA00``.
#. Upload a 320 by 240 1-bit bitmap to general VRAM.
#. Point layer 0 at that bitmap, choose 1-bit bitmap mode, and set both scale
   registers to 64.
#. Set VGA output and enable layer 0.

This yields a crisp full-frame image while using 9,600 bytes of VRAM.
