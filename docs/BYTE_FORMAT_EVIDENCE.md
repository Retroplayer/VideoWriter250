# Byte-level firmware and disk evidence

This document tracks **verified observations**, not a claim that the VideoWriter format is fully documented. Image and ROM samples are not redistributed.

## Physical format

The examined `VW1(2).img` and `videowriter(2).img` are both exactly 368,640 bytes: 80 tracks, one side, 18 256-byte sectors per track. They both have 64-byte user-directory records at track 0 sector 1, a least-significant-bit-first allocation bitmap beginning in track 0 sector 0, and document pointers in entry bytes +26/+27.

At disk offset `0xBE` the two files do **not** match. No universal `WV` identifier is established.

## VW1 single document (bytes verified against the image)

- Directory entry `Document1` is at image offset `0x100`. Its document pointer is `01 03`.
- Header at track 1, sector 3, begins `46 00 01 04`: **70** (little-endian) followed by reference to track 1 sector 4.
- Data sector at track 1, sector 4 has the following layout:

| Data-sector offset | Bytes | Observation |
| --- | --- | --- |
| `0x00..0x01` | `01 00` | Unknown record prefix |
| `0x02..0x1A` | 25 `00` bytes | Binary preamble |
| `0x1B` | `FF` | Marker immediately before visible text |
| `0x1C..0x27` | ASCII `Hello World!` | Recovered text (12 bytes) |
| `0x28..0x29` | `96 01` | Marker immediately after text |
| `0x2A..` | Mostly `00`; `FF 20` at `0x44..0x45` | Record/padding not yet interpreted |

**Important:** 70 is **not** the printable text length (12); it encompasses binary data. Any generic printer of printable bytes is not a verified text decoder.

A conservative decoder now recognizes **only this exact short-document envelope** and extracts the span from `0x1C` through the byte immediately before `96 01`. A complete raw `.bin` and annotated dump are also saved. This recognition is provisional for similar documents; other short-document variants are not presumed identical.

## Welcome letter

Its header at track 31/sector 1 begins `15 08` = 2,069 and references nine data sectors. Its initial data bytes are `00 00 02 04 9B` followed by `Dear`. Internal paragraph sequences include `99 00 00 02 04 9B`.

The `Welcome` and `Videowriter` directory entries reference the same header. `Letter` references an unallocated sector, so it cannot be confidently exported as an active file.

## Firmware cross-check: ROM1 (IC3)

The following are **file offsets within the 32 KiB ROM1 image**, not asserted CPU addresses.

| File offset | Z80 bytes | Established implication |
| --- | --- | --- |
| `0x1237` | `FE 96 CA D7 12` | Compares accumulator against `0x96` and conditionally jumps |
| `0x1600` | `FE 96 CA B5 16` | Additional specific handling of `0x96` |
| `0x34A1` | `FE 93 20 02 3E 92` | Detects `0x93` then substitutes `0x92` |
| `0x34AF` | `FE 91 20 0E` | Specific branch on `0x91` |
| `0x647C` | `FE 90 28 18 FE 91 28 1A FE 92` | Branches on members of `0x90..0x92` style/control family |
| `0x693C` | `FE 92 CA 87 69 FE 90 CA 87 69 FE 91` | Multiple tests on those style/control values |

These byte sequences confirm that ROM1 **deliberately distinguishes** the formatting and record codes found on disk. They do **not** alone prove their human-readable semantics. A proper disassembly needs control-flow and register-state tracing, especially across ROM calls.

ROM2 contains the integrated HELP system: bold, underline, superscript, subscript, centering and other formatting features are documented. The firmware code must be traced to connect UI functions to stored byte codes.

## Remaining gaps

Still not established: complete directory status semantics, disk metadata at track 1 sectors 0–2, the lengths and subrecords within short-document envelopes, deleted-file semantics, page-formatting structures, style token meanings, and text continuation across arbitrary document layouts.

Do not label the file format \"fully understood\" or claim lossless semantic TXT/RTF for arbitrary documents until these are proven.
