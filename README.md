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

The program calls `gw.exe`, then extracts documents into a newly created `recovered` directory inside the current folder. If that folder already exists, a numbered directory is chosen to avoid overwriting earlier results.

Documents are saved individually as TXT, RTF, annotated text, and lossless binary. **TXT is the priority; RTF formatting support is incomplete.** Do not edit or discard the original floppy.

**Important:** This is a development prototype. The drive-reading command has **not been verified with physical Greaseweazle hardware**. The decoding code has been tested only on a small number of known disk images. If the drive-reading command fails, save its printed output for troubleshooting.

## Recover from an existing decoded disk image

```cmd
vwrecover.exe mydisk.img extract recovered
```

The image must be the 360 KiB decoded sector image (80 cylinders × 1 head × 18 sectors × 256 bytes). An SCP flux capture is **not** a decoded sector image.

## Purpose

Recover old writings onto a modern computer with minimal steps. This program is **read-only**: it does not format, modify, or write to VideoWriter disks.
