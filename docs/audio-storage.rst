Audio and Storage
=================

VERA provides two independent audio engines and an SPI controller. The PSG and
PCM streams are mixed before reaching the stereo DAC.

Programmable sound generator
----------------------------

The PSG has 16 voices. Each voice occupies four bytes beginning at VRAM
``0x1F9C0 + 4*voice``.

.. list-table:: PSG voice record
   :header-rows: 1
   :widths: 14 86

   * - Byte
     - Fields
   * - 0
     - Frequency word bits 7:0
   * - 1
     - Frequency word bits 15:8
   * - 2
     - ``[7]`` right enable, ``[6]`` left enable, ``[5:0]`` volume
   * - 3
     - ``[7:6]`` waveform, ``[5:0]`` pulse width

The PSG sample clock is ``25 MHz / 512 = 48,828.125 Hz``. Calculate a frequency
word with:

.. code-block:: text

   frequency_word = round(frequency_hz * 131072 / 48828.125)

For example, 440 Hz is approximately 1181. Volume is logarithmic from 0
(silent) to 63 (loudest). Set either or both channel-enable bits to place the
voice in the stereo mix.

.. list-table:: PSG waveforms
   :header-rows: 1
   :widths: 20 30 50

   * - Code
     - Waveform
     - Width field
   * - 0
     - Pulse
     - Duty cycle; 63 is approximately square
   * - 1
     - Sawtooth
     - Reserved by the maintained core
   * - 2
     - Triangle
     - Reserved by the maintained core
   * - 3
     - Noise
     - Ignored

PCM playback
------------

PCM uses a 4 KiB FIFO. Samples are signed two's-complement values. The FIFO
accepts the following byte orders:

.. list-table:: PCM byte order
   :header-rows: 1
   :widths: 25 75

   * - Format
     - Bytes written to ``AUDIO_DATA``
   * - 8-bit mono
     - mono
   * - 8-bit stereo
     - left, right
   * - 16-bit mono
     - mono low, mono high
   * - 16-bit stereo
     - left low, left high, right low, right high

``AUDIO_RATE`` controls how often an input sample is consumed. A value of 128
gives 48,828.125 samples per second, 64 gives about 24,414, and 32 gives about
12,207. A value of zero stops consumption. Values above 128 are not valid
playback settings.

A clean startup sequence is:

#. Write ``AUDIO_RATE=0``.
#. Set ``AUDIO_CTRL.FIFO_RESET`` together with the desired format and volume.
#. Clear the reset bit while preserving the format and volume.
#. Fill an initial part of the FIFO through ``AUDIO_DATA``.
#. Enable the audio-low interrupt if desired.
#. Write the target sample rate.

Refill before the FIFO becomes empty. ``ISR.AFLOW`` becomes active when the
FIFO falls below one-quarter full. A write made while ``AUDIO_CTRL.FULL`` is
set is ignored.

SPI controller
--------------

The SPI controller shares its signals with the card socket. A byte transfer is
full duplex: every transmitted bit clocks one received bit.

.. code-block:: c

   uint8_t vera_spi_transfer(uint8_t value)
   {
       vera_write(0x1e, value);
       while (vera_read(0x1f) & 0x80) { }
       return vera_read(0x1e);
   }

Set ``SPI_CTRL.SELECT`` to assert chip select and clear it to release the card.
Use ``SLOW=1`` during card initialization; its approximately 390 kHz clock is
below the 400 kHz initialization limit used by many cards. Clear ``SLOW`` for
the 12.5 MHz operating clock after initialization.

For consecutive reads, ``AUTOTX`` reduces host overhead. Each read of
``SPI_DATA`` returns the last received value and starts a new transfer with
``0xFF`` as the transmitted byte. Account for this one-byte pipeline and wait
for ``BUSY`` before consuming the next result.

VERA supplies byte-level SPI transport, not a filesystem. Host software remains
responsible for card commands, block addressing, partition parsing, and any
filesystem implementation. Release chip select at the end of every command
transaction.
