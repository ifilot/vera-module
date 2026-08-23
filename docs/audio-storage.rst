Audio and Storage
=================

PSG audio
---------

The programmable sound generator has 16 channels, each occupying four bytes at
``$1:F9C0``. A channel contains a frequency value, left/right volume, and a
waveform selection. Pulse, sawtooth, triangle, and noise waveforms are
available. Set volume to zero to mute a channel cleanly.

PCM audio
---------

``AUDIO_CTRL`` at ``$9F3B`` configures the PCM FIFO. It reports empty and full
state, resets the FIFO, selects 8- or 16-bit samples, selects mono or stereo,
and sets PCM volume. ``AUDIO_RATE`` at ``$9F3C`` selects the sample rate;
``AUDIO_DATA`` at ``$9F3D`` accepts the sample stream. Feed the FIFO in small
bursts when it has space instead of waiting for it to empty.

SPI storage
-----------

The SPI controller is exposed at ``$9F3E`` (data) and ``$9F3F`` (control).
Write a byte to ``SPI_DATA`` to start a transfer, wait for ``SPI_CTRL.BUSY`` to
clear, then read ``SPI_DATA`` for the received byte. ``SPI_CTRL.SELECT``
asserts the SD-card chip select. The controller also supports automatic
transmit and a slow clock option for card initialisation.

Reliable card access follows the normal SPI-card sequence: begin with chip
select released, issue the card reset and identification commands at the slow
clock, then select the operational clock only after the card is ready. Always
release chip select at the end of a command transaction.
