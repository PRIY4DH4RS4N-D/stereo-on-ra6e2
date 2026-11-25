# Stereo Vision & MicroSD Card Subsystem on Renesas BGK-RA6E2

Embedded computer vision and SD card storage implementation on the **Renesas BGK-RA6E2** evaluation board featuring the **R7FA6E2BB3CFM** Arm Cortex-M33 MCU (200 MHz, 40 KB SRAM, 256 KB Flash).

---

## 📁 Repository Structure & Projects

This repository contains all projects developed for the RA6E2 embedded vision and storage system:

```
├── Stereo_ADAS/              # Main Project: SD-backed Stereo Vision Pipeline
│   ├── src/                  # Stereo matcher (Census 5x5, Hamming, Median 3x3), FatFs
│   ├── sd_card/              # 325 rectified stereo pairs (KITTI 2015 & synthetic)
│   ├── script/               # Python dataset packer (stereo_sd.py)
│   └── BGK_SD_Check/         # Fast SD bus validation utility
│
├── SDcard_Test/              # Project 1: MicroSD Card SPI & FatFs Verification
│   ├── src/                  # Bitbang SPI driver, FatFs, UART/RTT diagnostic logger
│   └── flash.bat             # One-click J-Link flash script
│
├── SDcard_Image_Store/       # Project 2: MicroSD PNG Decompressor & Memory Loader
│   ├── src/                  # Lightweight Tiny Inflate (mini_inflate), image_loader
│   └── flash.bat             # One-click J-Link flash script
│
└── README.md
```

---

## 1. Project Summaries

### 🚀 `Stereo_ADAS` — SD-Backed Stereo Vision Matching
- **Functionality**: Streams synchronized Left and Right $128 \times 128$ image pairs from an SD card (`STEREO.BIN`), computes dense stereo disparity maps using a $5 \times 5$ Census transform with Hamming distance cost aggregation and a $3 \times 3$ median filter, and saves the output disparity maps as standard BMP images on the SD card into timestamped run folders (`RUN00001/`).
- **Memory Optimization**: Fits inside 40 KB SRAM by maintaining only 5 sliding grayscale rows per camera and 3 disparity rows at any time.
- **Dataset**: Pre-packaged with 325 stereo pairs (64 real rectified KITTI 2015 scenes + 5 demo pairs + 256 synthetic multi-depth scenes).

### 🛠️ `SDcard_Test` — Standalone SD SPI & FatFs Driver
- **Functionality**: Robust SD card driver ported from ESP32 `SD.h` / `SPI.h` conventions to the Renesas RA6E2 hardware architecture.
- **Diagnostics**: Real-time pin diagnostics via live hardware register reads (`PmnPFS_b.PIDR`), detailed SPI command retry engine, dual-logging over UART (P411/Arduino D1 @ 115200 8N1) and SEGGER J-Link RTT Viewer.
- **Verification**: Automatically formats/mounts FatFs, writes `"hello from ra6e2"`, reads the data back, verifies byte-by-byte integrity, and asserts onboard LED status.

### 🖼️ `SDcard_Image_Store` — PNG Decompressor & RAM Visualizer
- **Functionality**: Reads compressed PNG images (e.g. `0:/LEFT/F0000001.PNG`, 11,919 bytes) directly from the SD card using ChaN FatFs.
- **Zero-Heap Decompression**: Contains an embedded RFC 1950/1951 Deflate/zlib decompressor (`mini_inflate.c`) that decompresses PNG `IDAT` chunks and strips filter bytes in-place.
- **Memory Inspection**: Uncompressed $128 \times 128$ 8-bit grayscale pixels (16,384 bytes) are stored in memory buffer:
  - **Variable**: `g_decoded_pixels`
  - **Address**: `0x20000AFC`
  - **Inspection**: Directly viewable using e² studio's **Raw Image Format** visualizer (128x128, Monochrome 8bpp).

---

## 🔌 Hardware Wiring & Pin Mapping

Connect the MicroSD card SPI module to the BGK-RA6E2 board as follows:

| MicroSD Pin | RA6E2 Pin | Board Location | Function |
|---|---|---|---|
| **VCC** | 5V / 3V3 | Arduino Power Header (CN1) | Power Supply |
| **GND** | GND | PMOD1 Pin 5 or GND Header | Ground |
| **CS** | `P301` / `P106` | PMOD1 Pin 1 / Arduino D10 | SPI Chip Select |
| **MOSI** | `P109` | PMOD1 Pin 2 / Arduino D11 | SPI Data In |
| **MISO** | `P110` | PMOD1 Pin 3 / Arduino D12 | SPI Data Out |
| **SCK** | `P111` | PMOD1 Pin 4 / Arduino D13 | SPI Clock |

### Onboard Status LEDs:
- **LED1 (P407)**: Red / Status Indicator
- **LED2 (P408)**: Green / Success Indicator
  - *Both Solid ON*: Operation Complete & Verified
  - *LED1 ON, LED2 OFF / Blinking*: Detection or Read/Write Error

---

## 💻 Building and Debugging in e² studio

1. **Import Projects**:
   - In e² studio: `File -> Import... -> General -> Existing Projects into Workspace`.
   - Browse to `c:\Renesas_Final` and import `Stereo_ADAS`, `SDcard_Test`, or `SDcard_Image_Store`.
2. **Build**:
   - Select project and press `Ctrl+B` (GCC Arm Embedded Toolchain).
3. **Debug / Flash**:
   - Use the pre-configured `.launch` or `.jlink` files to flash via the onboard J-Link debugger.
   - For image verification, pause execution and inspect `g_decoded_pixels` at `0x20000AFC` in e² studio's Raw Image Format dialog (128x128, Monochrome 8bpp, Line alignment 128 bytes).

---

## 📜 Serial & RTT Telemetry
- **Baud Rate**: `115200 8N1` (Arduino D1 / P411 or J-Link CDC COM Port)
- **RTT Viewer**: Connect using target `R7FA6E2BB`, Interface `SWD` @ 4000 kHz, Terminal 0.
