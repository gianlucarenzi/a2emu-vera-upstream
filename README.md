# a2emu — Apple II + A2VERA emulator

Reinette II plus (Apple ][+ / IIe, 65C02) with an A2VERA card: VERA video, PSG and the YM2151 at VERA base + $20.

- `make` builds `a2emu` (SDL2 window + audio) and `a2run` (headless test runner); `make windows` builds `a2emu.exe`.
- Put the Apple ROMs in `rom/` (`appleII+.rom`, `diskII.rom`, IIe 342-0135/0134 or 342-0303/0304, IIe video ROM 342-0133 or 342-0265 for lowercase/80 columns); they are not included.
- ROM source: the images used during development came from the Asimov archive mirror,
  <https://mirrors.apple2.org.za/ftp.apple.asimov.net/emulators/rom_images/>
  (`apple_ii+_rom.zip` -- its F800-FFFF replaced by `Apple II+ - 341-0020 - Applesoft BASIC Autostart Monitor F800 - 2716.bin` from the collection below: the image in the zip has the IRQ vector patched to $FF59 instead of $FA40, so IRQs never reach $03FE -- `Apple Disk II 16 Sector Interface Card ROM P5 - 341-0027.bin-with-D4-D7 data bits swapped.bin`
  as `diskII.rom` -- it must start with `A2 20 A0 00`: the file without that suffix is a raw chip
  dump with scrambled lines and never boots a disk -- and the IIe CPU and video ROMs from
  `APPLE Computer and Peripheral Card Roms Collection.zip`).
  **The Apple ROMs are copyrighted by Apple: downloading them is not legal unless you actually own
  the original hardware.** If you do, dumping the chips of your own machine is the clean way to get them.
- Run: `./a2emu --model iie disk.po` (from this directory; or `A2_MODEL=iie`); `iiee` = enhanced IIe, `--aux 64K` adds the extended 80-column card.
  Every option sets the `A2_*` variable of the same name and overrides it; `./a2emu --help` lists them.
  The window title shows the model (`Reinette ][+`, `Reinette //e`, `Reinette //e (enhanced)`) and the disks.
  Double-res video (IIe with aux memory only): with 80COL on and `$C05E` (AN3 off, DHIRES) the
  graphics modes become double hi-res (560 dots, 140 x 192 in 16 colours) and double lo-res (80 x 48);
  `$C05F` or a reset turns it off.  Without an aux card, and on the ][+, `$C05E` changes nothing.
  **`PR#3` (80 columns) needs `A2_AUX=64`**: the IIe defaults to no aux memory, and at reset the IIe
  firmware enables its own 80-column code at $C3xx only if it finds aux memory; otherwise it leaves
  the (empty) external slot 3 mapped and `PR#3` hangs, as on a real IIe without an 80-column card.
  `a2run` takes `--aux 64` for the same.
  Aux memory size: `./a2emu --aux SIZE disk.po` (`A2_AUX`; `a2run --aux SIZE`), SIZE =
  `0`, `64K` (extended 80-column card), `512K`, `1M`, `4M`, `8M` or `RAM2E`.  512K-8M fit a RamWorks
  III-compatible card: a write to `$C071`/`$C073`/`$C075`/`$C077` selects the 64 KB aux bank, reset
  selects bank 0, the 80-column screen always shows bank 0.
  `RAM2E` is Garrett's Workshop's RAM2E II (GW4203, 8 MB, MAX CPLD version), modelled on its HDL:
  besides the bank register it takes the command sequence (`FF 00 55 AA C1 AD` command argument
  on the bank register) for chip/LED/revision detection, the capacity mask, the LED and saving the
  settings, and it has no reset line (bank and settings survive a reset).  Its utility
  `GWRAM.SYSTEM` works: capacity, LED, Apple+number to save, R x25 to erase.  The saved settings go
  to `ram2e.nvm` next to `rom/` (`A2_RAM2E_NVM=FILE`; `a2run --ram2e-nvm FILE`, default: not saved).
  E.g. `./a2emu --model iiee --aux RAM2E GWRAM.po`.
- VERA slot: `A2_VERA_SLOT=<1-7>` (default 4; `a2run --vera-slot N`), e.g. `A2_VERA_SLOT=5 ./a2emu disk.po`
  puts the card at $C500-$C51F.  Slot 6 always holds the Disk II and slot 7 the hard disk when one is
  inserted: their firmware wins the reads there, so the VERA is unreachable there.
- `make install` copies binaries, `assets/` and `rom/` to `~/.local/share/a2emu-vera` and puts `a2emu`/`a2run`/`a2fs` in `~/bin` (override with `DATADIR=`/`BINDIR=`); `make uninstall` removes them but keeps the installed `rom/`.
- Keys: Ctrl+F1 / Alt+F1 save the floppy in drive 1 / 2 back to its file, F2 screenshot, F3 paste the clipboard as keystrokes,
  Shift/Ctrl+F4 volume, Shift/Ctrl+F5/F6 joystick release/action speed, Shift/Ctrl+F7 zoom,
  F9 warp, F10 pause, F11 reset, F12 help.
- SD card: `A2_SDCARD=sd.img` (or `a2run --sdcard sd.img`) puts a raw image of 512-byte sectors (at least 512 KB) on the VERA's SPI port as an SDHC card in SPI mode (CMD0/1/8/9/10/12/13/16/17/18/24/25/55/58/59, ACMD13/23/41/51; written through).  The SPI follows VERA 48.0.1's `spictrl.v` clock by clock: a byte takes 16 VERA clocks, 512 with SLOW.
- No-Slot Clock (IIe): a Dallas DS1216E SmartWatch under the CD ROM, read through the internal ROM at `$C100-$CFFF` (the usual driver: `STA $C00A`, `LDA $C304`, the 64-bit recognition pattern with `LDA $C300`/`$C301`, then 64 reads of `$C304` on D0; libvera's `plat_rtc`).  Its time never goes back: it starts from the date saved by the last run (`A2_NSC_FILE`, default `~/.config/a2emu-vera/nsc-clock.txt`, a line `YYYY-MM-DD HH:MM:SS`; 1980-01-01 00:00:00 without one), runs with the emulated CPU (a second every 1,020,484 cycles), is saved when the emulator exits, and a program setting it moves it forward only.  `A2_NSC=0`: no clock.
- VERA timing: the VERA catches up to the CPU before every register access and after every instruction (where its IRQ is sampled, as on the 6502); its audio makes a sample every 512 VERA clocks, so the PCM FIFO and AFLOW follow CPU time.  Writes to the composer/layer registers and, unless `A2_VERA_MIDLINE=0`, to VRAM/palette/sprites draw the line up to the beam first: raster effects land on the pixel (x16emu does the VRAM part only with `-midline-effects`).
- `--wav out.wav` (`A2_WAV`) records the stereo card audio, `--no-speaker` (`A2_SPEAKER=0`) drops the speaker stream, `--fps` (`A2_FPS=1`) prints the emulated clock and the VERA frame rate; `a2run --psgvlog FILE` logs the PSG registers at each VSYNC, before its IRQ.
- `make install` also installs `a2fs` and the SDK (`a2fs sdk` prints where), for cc65 programs
  booted straight from a Disk II floppy, no DOS:
  - `a2fs mkimage prog.dsk prog.bin [NAME=FILE ...]`: bootable image (`sdk/boot.s` loads the
    program at $0900 with the Disk II PROM's sector reader) with files in A2FS, a minimal file
    system (directory on track 17, contiguous files with a CRC-16); `a2fs ls|get|put|rm` work on
    images, `a2fs --help` has the layout.
  - link with `sdk/a2emu-vera.cfg`; for files on the Apple add `sdk/a2fs.c sdk/rwts.s` and
    `#include "a2fs.h"` (`fs_start`, `fs_mount`, `fs_read`, `fs_write`, `blk_read`, `blk_write`).
    `rwts.s` reads and writes with the real drive's timing.
  - in a project: `SDK := $(shell a2fs sdk)`, then
    `cl65 -t apple2 -C $(SDK)/a2emu-vera.cfg -I $(SDK) -Osir -Cl -o prog.bin main.c $(SDK)/a2fs.c $(SDK)/rwts.s`
    and `a2fs mkimage prog.dsk prog.bin DATA=data.bin`.
- `examples/sdtest` (`make test`): SD init, CSD, single/multiple block reads and writes over SPI, checked from the host.
- `examples/ramworks` (`make test`): IIe aux banking for every `--aux` size, and the RAM2E II's
  commands over three power-ons (detection, LED, capacity mask, command timeout, saved settings).
- `examples/dblres` (`make run`): 16 colour bars in double hi-res, then double lo-res, then plain
  hi-res with DHIRES off (a key steps through them).
- `examples/hello` (`make run`) and `examples/fstest` (`make test`: reads two files, writes a
  third and reads it back, then checks it from the host) use the SDK from this tree.
- The header of `a2run.c` lists the headless options (`--model`, `--aux`, `--hd`, `--frames`, `--keys`,
  `--input`, `--wav`, `--ymlog`, `--ym off`, ...).
- YM2151 at VERA base + $20 (`--ym off` / `A2_YM=0` removes it).  YM2151 core: ymfm (BSD-3, `extern/ymfm`).

A2VERA code: @fletto666 
YMFM integration: @sandlbn
