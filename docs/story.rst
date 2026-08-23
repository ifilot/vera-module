Origins and Continuation
========================

The original design
-------------------

VERA was conceived and implemented by Frank van den Hoef as a practical video
and I/O device for new retro-computing hardware. The name evolved with the
project: the first repository title used **Video Embedded Retro Adapter**, the
preliminary rev4 hardware manual used **Video Enhanced Retro Adapter**, and the
later programmer's reference uses **Versatile Embedded Retro Adapter**. This
manual follows the later name.

Its defining idea is unusually durable: put the timing-sensitive work in a
small FPGA, provide dedicated video memory, and expose the result through a
straightforward 8-bit peripheral bus.

The first public repository commit dates from June 2019. The design grew to
include two independently configured graphics layers, hardware sprites,
palette-based video, synthesized and sampled audio, and an SPI controller. It
remained useful outside any single host because the external contract is only
five register-select lines, eight data lines, control strobes, and an interrupt
output.

Project timeline
----------------

The preserved upstream Git history records its last commit on 20 October
2023. No further upstream development was recorded during 2024. Continuation
of the project began in 2025 under the neutral **Open VERA Module** name, with
two priorities: maintain the hardware and firmware, and make the complete
build and programming path depend more strongly on open-source tools.

That work is now represented by this repository. FPGA synthesis uses Yosys,
place-and-route uses nextpnr, and bitstream generation uses Project IceStorm.
The tool versions can be pinned in a container, allowing the same source to be
built locally and in continuous integration.

Compatibility
-------------

The maintained project treats compatibility with the original VERA module as
a design goal. Existing software should continue to see the familiar 32-byte
register window, 128 KiB VRAM organization, graphics pipeline, audio systems,
and SPI controller. Maintenance additions, such as the readable ecosystem
identifier, occupy otherwise unused display-controller banks.

Compatibility is verified against the maintained HDL, not inferred from a
particular host computer's address map. Release notes identify intentional
changes when one becomes necessary.
