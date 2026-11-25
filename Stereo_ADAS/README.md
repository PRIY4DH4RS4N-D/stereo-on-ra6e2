# SD-backed stereo vision on RA6E2

The BGK-RA6E2 streams predefined left/right pairs from a FAT32 SD card, runs 5×5 Census matching with Hamming distance and a 3×3 median filter on the MCU, and writes one directly viewable BMP disparity image to the same card for every pair. It searches 0–15 pixels at 128×128 resolution. The MCU keeps five grayscale rows per camera, three disparity rows, and sector buffers in SRAM.

## Ready-to-copy dataset

Open `sd_card`. It contains **325 predefined stereo pairs** in this order.

- `F0000001` through `F0000064` are real rectified [KITTI Stereo 2015 training pairs](https://www.cvlibs.net/datasets/kitti/eval_scene_flow.php?benchmark=stereo), scenes `000000_10` through `000063_10`.
- `F0000065` through `F0000069` are the five numbered reference-repository demo pairs.
- `F0000070` through `F0000325` are deterministic synthetic scenes with multiple depths and occlusions.

`LEFT` and `RIGHT` contain matching PNGs for viewing on the PC. `STEREO.BIN` contains those same pixel values packed for sequential reading by the MCU. `FRAMES.TXT` maps each frame ID to its source.

## Connect the SD module

Use a 3.3 V SPI microSD module with the BGK-RA6E2 PMOD1 connector in its default SPI/UART configuration. The USB card reader is used on the PC to copy files.

- SD CS → PMOD1 pin 1, P301.
- SD MOSI/DI → PMOD1 pin 2, P109.
- SD MISO/DO → PMOD1 pin 3, P110.
- SD SCK/CLK → PMOD1 pin 4, P111.
- SD GND → PMOD1 pin 5.
- SD VCC → the board's Arduino power header 3V3 pin. Confirm that the module supports 3.3 V supply and logic.

See [Renesas BGK-RA6E2 wiring and jumper diagrams](https://www.renesas.com/en/document/mat/qc-bekitpoc2z-manual).

## Process the SD card

1. Copy the **contents** of `sd_card` to the root of the FAT32 card. If the card still has the five-pair `STEREO.BIN`, replace it with the one from `sd_card`.
2. Safely eject the card from Windows and insert it in the SPI module.
3. Program `Debug/ADAS_Stereo.srec` or build and program from e² studio using the `ADAS_Stereo Debug_Flat` launch. Resume execution if the debugger stops at `main`.
4. LED1 lights while processing. LED2 stays lit on success and blinks on error. Each reset starts processing from the first pair and creates a new directory such as `RUN00001`, leaving earlier results available.
5. Once LED1 goes off, switch off the board and return the card to the PC. Open the BMPs in `RUN00001` with a standard image viewer. For example, `RUN00001/F0000001.BMP` matches `LEFT/F0000001.PNG` and `RIGHT/F0000001.PNG`. The complete 325-frame run uses about 5.2 MiB for the BMP files before FAT allocation overhead.

## Watch the run in PuTTY

Connect the board's micro-USB cable and locate **JLink CDC UART Port** under **Ports (COM & LPT)** in Windows Device Manager. Select **Serial** in PuTTY, enter that COM port, and use **115200 baud, 8 data bits, no parity, 1 stop bit, no flow control**. Open the session before pressing the board's Reset button. If e² studio halts the MCU after reset, press **F8** to resume.

The first line identifies the firmware. `[SD] Card detected` means the card answered the SPI initialization commands. `[FS] Mounted` means the filesystem could be read. `[DATA] Loaded 325 stereo pairs` confirms the input file passed validation. Each `[BMP] Saved` line appears after the board has closed and renamed that BMP on the card. Wait for `[RUN] Complete` before powering off and removing the card.

If `CMD0 response 0xFF` appears, check the SPI module's power, ground, and four PMOD1 signal wires. If the card is detected but the filesystem fails to mount, check that it is FAT32. If mounting succeeds but opening `STEREO.BIN` fails, check that the file is directly in the card root.

## Pack your own dataset

Install Pillow with `python -m pip install Pillow`. Use synchronized, rectified pairs with matching filenames in two directories. The packer also accepts matching stems ending in `_left` and `_right`. Pairs are processed in lexicographic order by stem. The tool converts both images to 128×128 grayscale. Aim for disparity within 0–15 pixels after resizing. Create the new dataset in a separate location, then copy it to the card root as `STEREO.BIN`.

```powershell
python script/stereo_sd.py pack --left C:\dataset\left --right C:\dataset\right --output C:\dataset\STEREO.BIN
python script/stereo_sd.py prepare --kitti C:\KITTI\training --synthetic 256 --output more_pairs
```

`STEREO.BIN` starts with a 512-byte header. Its first 16 bytes hold the eight-byte identifier `R6ST0001`, two little-endian 16-bit dimensions, and a little-endian 32-bit pair count. Each pair stores one left row followed by one right row for each image row. The BMP outputs store raw disparity as 8-bit palette indices 0–15, which display from black to white. Disparity is measured in pixels at the resized resolution. Unprocessed borders are encoded as zero.

For debugging, watch `g_stereo_status`, `g_stereo_frames_total`, `g_stereo_frames_done`, `g_stereo_fatfs_result`, and `g_stereo_output_name`. Status `1` indicates work in progress and `2` indicates success. Negative values are `-1` mount failed, `-2` input open failed, `-3` invalid input format or size, `-4` output open failed, `-5` input read failed, and `-6` output write or finalization failed.

The matcher is adapted from [Stereo_Vision_RA6E2 at commit 837b7a1](https://github.com/Senthil7271/Stereo_Vision_RA6E2/tree/837b7a14869bdcf3746be4cc2f2eb1604d7ab821). The source under `src/fatfs` uses ChaN FatFs R0.16 patch 2 from the [pinned FatFs mirror](https://github.com/abbrev/fatfs/tree/ab992fcf2926b8a1ee3f0a8f9cefb6584174100d/source).
