# VideoWriter 250 document recovery

Read-only Windows console utility intended to rescue writings from Magnavox VideoWriter 250 floppy disks. The original disk is never modified.

## Recover documents from a floppy disk (Windows)

1. Download the latest Windows build artifact from the [Actions page](https://github.com/Retroplayer/VideoWriter250/actions/workflows/windows.yml).
2. Extract `vwrecover.exe` **into the same directory as Greaseweazle's `gw.exe`**.
3. Connect a Greaseweazle and floppy drive, and insert a VideoWriter floppy disk.
4. In File Explorer, navigate to the folder containing `gw.exe`. Click the address bar, enter `cmd`, and press Enter to open Command Prompt **in that folder**.
5. Run:

   ```cmd
   vwrecover.exe drive extract recovered
   ```

Drive **0** is selected by default. For a second drive, supply its number immediately after `drive`:

```cmd
vwrecover.exe drive 1 extract recovered
```

Greaseweazle also accepts drive selectors `2`, `A`, and `B` (depending on the floppy cable and drive configuration). The supplied selector is passed as `--drive=<selector>` to `gw.exe`.

The program calls `gw.exe`, then extracts documents into a newly created `recovered` directory inside the current folder. If that folder already exists, a numbered directory is chosen to avoid overwriting earlier results.

Documents are saved individually as TXT, RTF, annotated text, and lossless binary. **TXT is the priority; RTF formatting support is incomplete.** Do not edit or discard the original floppy.

**Important:** This is a development prototype. The drive-reading command has **not been verified with physical Greaseweazle hardware**. The decoding code has been tested only on a small number of known disk images. If the drive-reading command fails, save its printed output for troubleshooting.

## Recover from an existing decoded disk image

```cmd
vwrecover.exe mydisk.img extract recovered
```

The image must be the 360 KiB decoded sector image (80 cylinders × 1 head × 18 sectors × 256 bytes). An SCP flux capture is **not** a decoded sector image.

## Extract from an ImageDisk IMD capture

ImageDisk captures made on a real PC floppy controller can be used directly:

```cmd
vwrecover.exe grandma.imd extract recovered
```

The IMD loader accepts **250 kbps MFM, single-sided, 256-byte sectors**, sorts physical sector IDs 0–17 into the 360 KiB logical image, and handles ImageDisk's compressed repeated-byte sectors and optional sector maps. Missing and CRC-bad sectors are **not** assumed to be valid zero-filled sectors. If a document references one of those sectors, it is skipped rather than silently exported as complete. Other intact documents remain recoverable.

This program does not communicate with the floppy controller directly through ImageDisk: use the original ImageDisk program to make an IMD capture, then give the IMD file to vwrecover.

## Purpose

Recover old writings onto a modern computer with minimal steps. This program is **read-only**: it does not format, modify, or write to VideoWriter disks.
