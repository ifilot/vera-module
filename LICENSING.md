# Licensing

## Purpose and project lineage

Open VERA Module is a community-maintained fork of the original VERA Module
created by Frank van den Hoef.  The original project was released under the
MIT License; it was already open source.  This project continues to preserve
that history, attribution, and the original VERA compatibility goal while
placing new, maintained work under reciprocal licences appropriate to its
kind.

This document is a practical guide to the licences in this repository, not
legal advice.  See [NOTICE](NOTICE) for attributions.

## Licence map

The repository is intentionally **multi-licensed by component**.  The licence
that applies to a file is determined by the most specific notice in that file
or directory; this table is the default where no more specific notice exists.

| Material | Default licence for maintained-fork contributions | Licence text |
| --- | --- | --- |
| Hardware design: `pcb/`, `firmware/source/`, constraint files, and HDL design documentation | CERN Open Hardware Licence v2 — Strongly Reciprocal (`CERN-OHL-S-2.0`) | [LICENSES/CERN-OHL-S-2.0.txt](LICENSES/CERN-OHL-S-2.0.txt) |
| Software: `driver/`, `toolchain/`, build scripts, utilities, programmer firmware, and CI configuration | GNU General Public License v3.0 or later (`GPL-3.0-or-later`) | [LICENSES/GPL-3.0-or-later.txt](LICENSES/GPL-3.0-or-later.txt) |
| Historic upstream material and any file expressly marked `MIT` | MIT | [LICENSE](LICENSE) |
| Third-party material | Its own accompanying notice or licence | See the relevant file or directory |

`CERN-OHL-S-2.0` is deliberately used for the hardware design: when a person
distributes a modified design or a product based on it, the corresponding
design documentation must remain available under the same reciprocal terms.
`GPL-3.0-or-later` provides the corresponding protection for software.

The Source Location for CERN-OHL-S purposes is the public repository from
which the relevant release was obtained.  A release must state its exact
repository URL and tag, and must make the complete modifiable KiCad and HDL
sources available there.

## The MIT upstream is retained

The root [LICENSE](LICENSE) is Frank van den Hoef's original MIT licence and
is retained verbatim.  It applies to the original material distributed under
it, including material that has since been moved or modified.  Its copyright
and permission notice must remain with substantial copies.

MIT permissions already granted for upstream releases cannot be withdrawn.
Accordingly, these reciprocal licences do not make historic upstream releases
non-MIT and do not claim ownership of Frank's or any other contributor's
work.  In a mixed file, the inherited MIT permission remains available for
the inherited material; the applicable reciprocal licence covers the
maintained-fork contribution to that file.

The maintainers may offer a complete fork release under the licence map above
only to the extent that they have the necessary rights.

## Attribution and notices

Do not remove or alter:

- Frank van den Hoef's MIT copyright and permission notice;
- attribution to other original contributors listed in [NOTICE](NOTICE);
- SPDX identifiers, file headers, and third-party notices; or
- the licence texts in `LICENSES/`.

The name "Open VERA Module" describes this maintained fork.  It does not
imply that the original VERA Module was closed, nor does it imply endorsement
or affiliation by Frank van den Hoef or any other upstream contributor.

## Releases, products, and binaries

When distributing a board, FPGA bitstream, programmer binary, or other
non-source release, include or link to:

1. the applicable licence texts and notices;
2. the exact source tag or commit and its public Source Location;
3. the complete preferred form for modification, including KiCad sources,
   HDL, constraints, build scripts, and programmer sources as applicable;
4. a clear notice of local modifications and their date; and
5. all required notices for third-party parts.

Do not represent a modified product as an official or endorsed Frank van den
Hoef VERA product.

## Contributions after this policy

New contributors must have the right to submit their work and license it
under the licence applicable to the component they modify.  Contributions to
hardware-design paths are submitted under `CERN-OHL-S-2.0`; contributions to
software paths are submitted under `GPL-3.0-or-later`.  See
[CONTRIBUTING.md](CONTRIBUTING.md).  A contributor may retain copyright in
their contribution; no copyright assignment is requested.
