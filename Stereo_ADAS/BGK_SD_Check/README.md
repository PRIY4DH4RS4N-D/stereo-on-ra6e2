# BGK-RA6E2 read-only SD card check

This is a separate e² studio project. It initializes the SD card over SPI, reads sector 0 with CRC validation, and mounts the FAT filesystem. If `STEREO.BIN` is at the card root, it reads up to 512 bytes from the file as an additional check. The firmware contains no SD write path.

## Connect and run

1. With the board unplugged, connect the 3.3 V SPI SD module to PMOD1. Connect CS to pin 1 (P301), MOSI/DI to pin 2 (P109), MISO/DO to pin 3 (P110), SCK/CLK to pin 4 (P111), and GND to pin 5. Connect VCC to the board's Arduino power header 3V3 pin if the module supports 3.3 V power and logic. See [Renesas' PMOD1 diagram](https://www.renesas.com/en/document/mat/qc-bekitpoc2z-manual#page=5).
2. Insert the card and connect the board using its micro-USB data cable.
3. Import this directory as an existing e² studio project, build Debug, and launch **BGK_SD_Check Debug_Flat**. The standalone programming image is `Debug/BGK_SD_Check.srec`.
4. Open the board's **JLink CDC UART Port** in PuTTY at **115200 baud, 8 data bits, no parity, 1 stop bit, no flow control**. Resume with **F8** if the debugger stops at `main`. Press the board's Reset button to restart the test.

The result repeats every five seconds. LED2 stays solid for a pass and blinks for a failure. A pass with `STEREO.BIN` typically looks like this.

```text
[SD] Card detected (SDHC/SDXC)
[CHECK] Raw sector 0 read OK. Mounting FAT volume
[CHECK] FAT volume mounted. Looking for optional /STEREO.BIN
[PASS] SD detected, sector read OK, FAT mounted. Read 512 bytes from /STEREO.BIN.
```

If you are using the debugger, inspect `g_sd_check_result`. A value of `1` means FAT mounted without a data file sample. A value of `2` confirms a successful file read. A negative value indicates a failure at detection (`-1`), sector read (`-2`), FAT mount (`-3`), or file read (`-4`).
