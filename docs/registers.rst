Register Reference
==================

The five address pins select a register by **offset**. Offset ``0x00`` means
``A4-A0 = 00000``; offset ``0x1F`` means ``A4-A0 = 11111``. No absolute host
address is implied anywhere in this chapter.

Quick map
---------

.. list-table:: Register offsets
   :header-rows: 1
   :widths: 12 24 12 52

   * - Offset
     - Name
     - Access
     - Purpose
   * - ``0x00``
     - ``ADDRx_L``
     - R/W
     - Selected VRAM pointer, bits 7:0
   * - ``0x01``
     - ``ADDRx_M``
     - R/W
     - Selected VRAM pointer, bits 15:8
   * - ``0x02``
     - ``ADDRx_H``
     - R/W
     - Pointer increment, direction, and address bit 16
   * - ``0x03``
     - ``DATA0``
     - R/W
     - VRAM data port 0
   * - ``0x04``
     - ``DATA1``
     - R/W
     - VRAM data port 1
   * - ``0x05``
     - ``CTRL``
     - R/W
     - Reconfigure, display bank, and pointer selection
   * - ``0x06``
     - ``IEN``
     - R/W
     - Interrupt enables and line/scanline bit 8
   * - ``0x07``
     - ``ISR``
     - R/W1C
     - Interrupt status and collision groups
   * - ``0x08``
     - ``IRQLINE_L`` / ``SCANLINE_L``
     - W/R
     - Interrupt line on write; current scanline on read
   * - ``0x09-0x0C``
     - Display-controller bank
     - R/W
     - Meaning selected by ``CTRL.DCSEL``
   * - ``0x0D-0x13``
     - Layer 0
     - R/W
     - Mode, memory bases, and scrolling
   * - ``0x14-0x1A``
     - Layer 1
     - R/W
     - Mode, memory bases, and scrolling
   * - ``0x1B-0x1D``
     - PCM audio
     - Mixed
     - FIFO control, sample rate, and data
   * - ``0x1E-0x1F``
     - SPI
     - R/W
     - Transfer data and controller state

VRAM pointers and data ports
----------------------------

VERA has two independent 17-bit VRAM pointers. ``CTRL.ADDRSEL`` selects which
pointer is visible in ``ADDRx_L``, ``ADDRx_M``, and ``ADDRx_H``. ``DATA0``
always uses pointer 0 and ``DATA1`` always uses pointer 1, regardless of
``ADDRSEL``.

``ADDRx_H`` has this format:

.. list-table:: ``ADDRx_H`` (offset ``0x02``)
   :header-rows: 1
   :widths: 18 24 58

   * - Bits
     - Field
     - Meaning
   * - 7:4
     - ``INCREMENT``
     - Increment code applied after each data-port access
   * - 3
     - ``DECR``
     - ``0`` adds the increment; ``1`` subtracts it
   * - 2:1
     - Reserved
     - Write zero; reads return zero
   * - 0
     - ``ADDRESS[16]``
     - Most-significant bit of the selected VRAM address

.. list-table:: Pointer increments
   :header-rows: 1
   :widths: 20 30 20 30

   * - Code
     - Bytes
     - Code
     - Bytes
   * - ``0``
     - 0
     - ``8``
     - 128
   * - ``1``
     - 1
     - ``9``
     - 256
   * - ``2``
     - 2
     - ``10``
     - 512
   * - ``3``
     - 4
     - ``11``
     - 40
   * - ``4``
     - 8
     - ``12``
     - 80
   * - ``5``
     - 16
     - ``13``
     - 160
   * - ``6``
     - 32
     - ``14``
     - 320
   * - ``7``
     - 64
     - ``15``
     - 640

The 40, 80, 160, 320, and 640-byte steps are convenient for column-wise
access to common row widths. See :doc:`vram` for transfer examples and read
prefetch behavior.

.. _reg-control:

Control register
----------------

.. list-table:: ``CTRL`` (offset ``0x05``)
   :header-rows: 1
   :widths: 18 24 58

   * - Bits
     - Field
     - Meaning
   * - 7
     - ``RESET``
     - Write ``1`` to request FPGA reconfiguration. Reads return ``0``.
   * - 6:1
     - ``DCSEL``
     - Select the meaning of display registers ``0x09-0x0C``.
   * - 0
     - ``ADDRSEL``
     - Select pointer 0 or pointer 1 for ``ADDRx_L/M/H``.

Preserve ``ADDRSEL`` when changing ``DCSEL`` if another routine may be using a
VRAM pointer. Reconfiguration resets the control state and reloads the default
palette.

.. _reg-ien:

Interrupt enable
----------------

.. list-table:: ``IEN`` (offset ``0x06``)
   :header-rows: 1
   :widths: 14 25 61

   * - Bit
     - Field
     - Meaning
   * - 7
     - ``IRQLINE[8]``
     - High bit of the programmed raster interrupt line
   * - 6
     - ``SCANLINE[8]``
     - Read-only high bit of the current scanline
   * - 5:4
     - Reserved
     - Write zero; reads return zero
   * - 3
     - ``AFLOW``
     - Enable audio FIFO-low interrupt
   * - 2
     - ``SPRCOL``
     - Enable sprite-collision interrupt
   * - 1
     - ``LINE``
     - Enable programmed-line interrupt
   * - 0
     - ``VSYNC``
     - Enable vertical-sync interrupt

Write the low eight bits of the requested line to offset ``0x08`` and bit 8 to
``IEN[7]`` before enabling ``LINE``. Reading offset ``0x08`` returns the low
eight bits of the current scanline; ``IEN[6]`` supplies its bit 8.

.. _reg-isr:

Interrupt status
----------------

.. list-table:: ``ISR`` (offset ``0x07``)
   :header-rows: 1
   :widths: 14 25 61

   * - Bits
     - Field
     - Meaning
   * - 7:4
     - ``COLLISIONS``
     - Read-only OR of colliding sprite-group masks
   * - 3
     - ``AFLOW``
     - Live indication that the PCM FIFO is less than one-quarter full
   * - 2
     - ``SPRCOL``
     - Latched sprite-collision event
   * - 1
     - ``LINE``
     - Latched programmed-line event
   * - 0
     - ``VSYNC``
     - Latched vertical-sync event

Write a one to bits 2, 1, or 0 to clear that latched event. Writing zero leaves
it unchanged. ``AFLOW`` clears only after sufficient PCM data has been added.

Display-controller banks
------------------------

Offsets ``0x09-0x0C`` are banked. Select a bank by writing ``DCSEL`` in
``CTRL``.

Bank 0: output and scaling
^^^^^^^^^^^^^^^^^^^^^^^^^^

.. list-table:: ``DCSEL = 0``
   :header-rows: 1
   :widths: 14 24 62

   * - Offset
     - Name
     - Fields
   * - ``0x09``
     - ``DC_VIDEO``
     - ``[7]`` current field (read-only), ``[6]`` sprites enable, ``[5]``
       layer 1 enable, ``[4]`` layer 0 enable, ``[3]`` reserved, ``[2]``
       chroma disable, ``[1:0]`` output mode
   * - ``0x0A``
     - ``DC_HSCALE``
     - Horizontal fractional scale; 128 is 1:1 and 64 is 2x
   * - ``0x0B``
     - ``DC_VSCALE``
     - Vertical fractional scale; 128 is 1:1 and 64 is 2x
   * - ``0x0C``
     - ``DC_BORDER``
     - Palette index used outside the active area

Output modes are ``0`` disabled, ``1`` VGA, ``2`` composite/S-video, and ``3``
15 kHz RGB with composite sync. ``CHROMA_DISABLE`` suppresses chroma on the
analog encoded outputs. Bit 3 is reserved in the maintained firmware and must
be written as zero.

Bank 1: active area
^^^^^^^^^^^^^^^^^^^

.. list-table:: ``DCSEL = 1``
   :header-rows: 1
   :widths: 14 24 62

   * - Offset
     - Name
     - Meaning
   * - ``0x09``
     - ``DC_HSTART``
     - Active-area start X, bits 9:2; multiply the byte by 4
   * - ``0x0A``
     - ``DC_HSTOP``
     - Active-area stop X, bits 9:2; multiply the byte by 4
   * - ``0x0B``
     - ``DC_VSTART``
     - Active-area start Y, bits 8:1; multiply the byte by 2
   * - ``0x0C``
     - ``DC_VSTOP``
     - Active-area stop Y, bits 8:1; multiply the byte by 2

Coordinates use the native 640 x 480 display space. A full active area is
``HSTART=0``, ``HSTOP=160``, ``VSTART=0``, and ``VSTOP=240`` as register
values, representing coordinates 0, 640, 0, and 480 respectively.

.. _firmware-id:

Banks 60-63: ecosystem identifier
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Select ``DCSEL`` 60, 61, 62, and 63 in order and read offsets ``0x09`` through
``0x0C`` from each bank. Concatenate the resulting 16 bytes and stop at the
first NUL byte. The result is an ASCII identifier derived from the repository
``VERSION`` file, for example ``VERA v0.1.0``.

Writes to offsets ``0x09-0x0C`` are ignored in these banks. Other unimplemented
banks read as zero and ignore writes.

Layer registers
---------------

Layer 0 occupies offsets ``0x0D-0x13``; layer 1 has the identical layout at
``0x14-0x1A``.

.. list-table:: Layer register layout
   :header-rows: 1
   :widths: 16 22 62

   * - L0 / L1
     - Name
     - Meaning
   * - ``0x0D`` / ``0x14``
     - ``Lx_CONFIG``
     - ``[7:6]`` map height, ``[5:4]`` map width, ``[3]`` attribute mode,
       ``[2]`` bitmap mode, ``[1:0]`` color depth
   * - ``0x0E`` / ``0x15``
     - ``Lx_MAPBASE``
     - Map address bits 16:9; base is aligned to 512 bytes
   * - ``0x0F`` / ``0x16``
     - ``Lx_TILEBASE``
     - ``[7:2]`` data address bits 16:11, ``[1]`` tile height, ``[0]`` tile
       width; base is aligned to 2048 bytes
   * - ``0x10`` / ``0x17``
     - ``Lx_HSCROLL_L``
     - Horizontal scroll bits 7:0
   * - ``0x11`` / ``0x18``
     - ``Lx_HSCROLL_H``
     - Horizontal scroll bits 11:8 in the low nibble
   * - ``0x12`` / ``0x19``
     - ``Lx_VSCROLL_L``
     - Vertical scroll bits 7:0
   * - ``0x13`` / ``0x1A``
     - ``Lx_VSCROLL_H``
     - Vertical scroll bits 11:8 in the low nibble

Color-depth codes ``0-3`` select 1, 2, 4, or 8 bits per pixel. Map width and
height codes ``0-3`` select 32, 64, 128, or 256 tiles. Tile width and height
bits select 8 pixels when clear and 16 pixels when set. See :doc:`graphics`
for the mode-dependent meaning of the attribute and scroll fields.

PCM audio registers
-------------------

.. list-table:: PCM registers
   :header-rows: 1
   :widths: 14 24 62

   * - Offset
     - Name
     - Fields
   * - ``0x1B``
     - ``AUDIO_CTRL``
     - Read: ``[7]`` full, ``[6]`` empty. Write: ``[7]`` FIFO reset.
       ``[5]`` 16-bit, ``[4]`` stereo, ``[3:0]`` logarithmic volume.
   * - ``0x1C``
     - ``AUDIO_RATE``
     - Playback rate, ``0`` stopped through ``128`` maximum valid rate
   * - ``0x1D``
     - ``AUDIO_DATA``
     - Write-only byte stream into the 4 KiB FIFO; reads return zero

Writes made while the full flag is set are ignored. :doc:`audio-storage`
describes sample formats, rates, and a reliable startup sequence.

SPI registers
-------------

.. list-table:: SPI registers
   :header-rows: 1
   :widths: 14 24 62

   * - Offset
     - Name
     - Fields
   * - ``0x1E``
     - ``SPI_DATA``
     - Write starts an 8-bit transfer; read returns the last received byte
   * - ``0x1F``
     - ``SPI_CTRL``
     - ``[7]`` busy (read-only), ``[6:3]`` reserved, ``[2]`` auto-transmit,
       ``[1]`` slow clock, ``[0]`` chip select

Writing ``SELECT=1`` asserts the card's active-low chip select. Writing
``SELECT=0`` releases it. The fast clock is 12.5 MHz; the slow initialization
clock is approximately 390 kHz. With auto-transmit enabled, reading
``SPI_DATA`` returns the previous receive byte and immediately begins another
transfer sending ``0xFF``.
