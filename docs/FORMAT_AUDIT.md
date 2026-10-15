# VideoWriter 250 disk format audit

The current console extractor is a **preliminary read-only recovery tool**, not a complete document decoder. This document distinguishes byte-level facts from hypotheses.

## Disk-level findings

- Decoded disk images: 368,640 bytes, consistent with 80 tracks, one side, 18 MFM sectors/track and 256 bytes/sector.
- In track 0, sector 0, bytes 0..179 function as a 1440-bit allocation map, one least-significant-bit-first bit per sector. Set = allocated.
- `VW1(2).img` has 23 allocated sectors (LBAs 0..22); `videowriter(2).img` has 568.
- Offset `0xBE` is **not a fixed `WV` signature**: VW1 contains `35 46`, the Welcome disk `57 56`. Its actual function is unknown.
- User directory records occupy track 0, sector 1, at 64 bytes/record. A system directory exists at track 0, sector 16 in the Welcome image.
- Directory name: bytes +6 through +25, 20 characters. Pointer: bytes +26,+27 = track, sector ID.

## Documents

### VW1 `Document1`

- User entry bytes 0..5: `01 01 00 00 01 00`; name `Document1`; pointer `01 03`.
- Header at track 1, sector 3 begins `46 00 01 04`: candidate length 70, data sector track 1 sector 4.
- That data sector begins `01 00`, 25 more zero bytes, and `FF`, then `Hello World!` at sector byte **28 (0x1C)**, followed by `96 01`. The text does not start at offset zero. The internal record framing is not fully decoded.

### Welcome / Videowriter

- Both user directory entries point to track 31, sector 1.
- That header begins `15 08` (candidate length 2069) and contains nine pointers: 31:2 and 31:4 through 31:11.
- Data begins with control sequences including `00 00 02 04 9B`. A repeating paragraph-like sequence is `99 00 00 02 04 9B`.
- Formatting controls `91`, `93`, `95` are observed but **not mapped to verified firmware behaviors**.
- Other apparent document records at 31:12 and 31:13 are **unallocated**, so they must not be treated as unquestionably active.

## Firmware evidence

- Original ROM1 (IC3) and ROM2 (IC4): each 32768 bytes. ROM2 contains HELP text identifying user-facing styling features including bold, underline, superscript, and subscript. It does not alone establish on-disk control-byte mapping.

## Unresolved and necessary before declaring the decoder complete

1. Sector-0 metadata beyond bitmap, including bytes at `0xB4..0xBF`.
2. Directory state fields +0..+5, deleted and alias records, directory extension.
3. Meaning of header byte count and chaining when documents exceed one header's pointer capacity.
4. Internal text record format, paragraph controls, page layout, tabs and style controls.
5. Recoverability of fragmented, damaged and deleted documents.
6. Firmware routine cross-reference confirming all interpretations, tested against distinct image specimens.

**Policy:** Original disks are never written. Uninterpreted bytes must not be silently discarded or replaced in lossless recovery output.
