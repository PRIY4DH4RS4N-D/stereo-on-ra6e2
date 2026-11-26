# RA6E2 Ultrasonic Distance Measurement System (HC-SR04)

Complete, modular, hardware-ready embedded firmware for the **Renesas BGK-RA6E2** development board (MCU: **R7FA6E2BB3CFM**, Arm Cortex-M33 @ 200 MHz, 40 KB SRAM, 256 KB Flash) interfacing with the **HC-SR04** ultrasonic distance sensor.

---

## 1. Project Overview

This project implements a precision ultrasonic distance measurement system using the Renesas Flexible Software Package (FSP) and Arm Cortex-M33 hardware cycle timing.
- Emits a clean **10 µs trigger pulse** on GPIO output (`P105` / Arduino D2).
- Uses the **32-bit hardware DWT (Data Watchpoint and Trace) cycle counter** running at **200 MHz** (5 ns resolution) to measure the returned ECHO pulse width.
- Converts the measured pulse duration to distance using the standard speed-of-sound formula:
  $$\text{Distance (cm)} = \frac{\text{Echo pulse duration (µs)} \times 0.0343}{2}$$
- Transmits standardized telemetry reports every **150 ms** over **UART** (115200 8N1) and **SEGGER J-Link RTT Viewer**.
- Implements non-blocking timeout recovery on both rising and falling edges, ensuring the firmware never hangs even if the sensor is disconnected or defective.
- Includes an automated mathematical self-test verifying formula accuracy on startup.

---

## 2. Hardware Connections & Wiring

### ⚠️ IMPORTANT SAFETY NOTE: 5V-to-3.3V Voltage Divider
The HC-SR04 sensor operates from 5V and outputs a **5V logic signal** on its **ECHO** pin. The Renesas RA6E2 MCU I/O pins are rated for **3.3V logic** (Absolute maximum rating: $V_{CC} + 0.3\text{V} \approx 3.6\text{V}$).

> [!CAUTION]
> **NEVER connect the HC-SR04 5V ECHO output directly to an RA6E2 GPIO pin.**
> You must route the ECHO signal through a resistive voltage divider to step the voltage down to ~3.3V.

### Voltage Divider Circuit
```
   HC-SR04 ECHO (5V)
          │
         [R1]  (1 kΩ or 2.2 kΩ)
          │
          ├───► To BGK-RA6E2 Arduino D3 (P408)  [~3.3V Safe Logic Level]
          │
         [R2]  (2 kΩ or 4.7 kΩ)
          │
         GND
```

#### Resistor Options:
- **Option A (Recommended)**: $R_1 = 1.0\text{ k}\Omega$, $R_2 = 2.0\text{ k}\Omega$
  $$V_{out} = 5.0\text{ V} \times \frac{2.0\text{ k}\Omega}{1.0\text{ k}\Omega + 2.0\text{ k}\Omega} = 3.33\text{ V}$$
- **Option B**: $R_1 = 2.2\text{ k}\Omega$, $R_2 = 4.7\text{ k}\Omega$
  $$V_{out} = 5.0\text{ V} \times \frac{4.7}{2.2 + 4.7} \approx 3.40\text{ V}$$
- **Option C**: $R_1 = 10\text{ k}\Omega$, $R_2 = 10\text{ k}\Omega$
  $$V_{out} = 5.0\text{ V} \times \frac{10}{20} = 2.50\text{ V} \quad (\text{Safely above the RA6E2 } V_{IH} \text{ threshold of } 2.31\text{ V})$$

---

### Hardware Wiring Table

| HC-SR04 Pin | RA6E2 Board Pin | Header Location | Description |
|---|---|---|---|
| **VCC** | **5V** | Arduino Power Header CN1 Pin 5 | 5V Power Supply |
| **GND** | **GND** | Arduino Power Header CN1 Pin 6 or 7 | Common Ground |
| **TRIG** | **P105** | Arduino Header **D2** | GPIO Output (10 µs Pulse) |
| **ECHO** | **P408** (via divider) | Arduino Header **D3** | GPIO Input (Pulse Capture) |

### Telemetry & Indicator Pins:
| Signal | RA6E2 Pin | Header Location | Function |
|---|---|---|---|
| **UART TX** | **P411** | Arduino Header **D1** | 115200 Baud 8N1 Serial Stream |
| **UART TX (Alt)** | **P101** | PMOD2 Pin 6 / SCI0 TX | Secondary Serial Stream |
| **LED1 (Red)** | **P104** | Onboard LED1 | Measurement Trigger Activity |
| **LED2 (Green)**| **P112** | Onboard LED2 | Valid Echo / Success Indicator |

---

## 3. Software Architecture

The project is structured into clean, decoupled, modular layers:

```
RA6E2_Ultrasonic_HCSR04/
├── src/
│   ├── hal_entry.c             # Application entry point, setup, and 150 ms loop
│   ├── hal_warmstart.c         # FSP warm-start hook
│   ├── timer_measurement.h     # High-resolution hardware timer interface
│   ├── timer_measurement.c     # Cortex-M33 DWT cycle counter (5 ns resolution)
│   ├── uart_logger.h           # Dual-channel UART and RTT telemetry interface
│   ├── uart_logger.c           # Bitbang UART (P411/D1) + SEGGER RTT engine
│   ├── ultrasonic_sensor.h     # HC-SR04 driver interface, config, and enums
│   └── ultrasonic_sensor.c     # Trigger generation, edge detection, timeout recovery
│
├── ra_gen/                     # FSP generated code (pin_data, clock_cfg, main.c)
├── ra_cfg/                     # FSP configuration headers
├── ra/                         # Renesas FSP BSP and CMSIS-6 core libraries
├── script/                     # Linker scripts (fsp.ld)
├── Debug/                      # Build outputs (ELF, HEX, SREC, MAP)
├── flash.bat                   # One-click J-Link flasher
├── flash.jlink                 # J-Link Commander flashing script
└── README.md                   # This document
```

---

## 4. Measurement Pipeline & Distance Calculation

1. **Trigger Generation**:
   - `TRIG` line held LOW for 4 µs.
   - Set HIGH for exactly **10 µs** using `timer_delay_us(10)`.
   - Set LOW.
2. **Rising Edge Detection**:
   - Loops until `ECHO` goes HIGH.
   - Protected by `rising_timeout_us` (5,000 µs = 5 ms).
   - If timeout expires, returns `ULTRASONIC_ERR_RISING_TIMEOUT` (Sensor disconnected or failed).
3. **Hardware Timestamping**:
   - At rising edge: `echo_start = DWT->CYCCNT;`
4. **Falling Edge Detection**:
   - Loops while `ECHO` remains HIGH.
   - Protected by `falling_timeout_us` (35,000 µs = 35 ms $\approx 600\text{ cm}$).
   - If timeout expires, returns `ULTRASONIC_ERR_FALLING_TIMEOUT` (No reflection).
5. **Pulse Width Calculation**:
   $$\Delta\text{Cycles} = \text{echo\_end} - \text{echo\_start}$$
   $$\text{Echo Duration (µs)} = \frac{\Delta\text{Cycles}}{\text{Cycles per µs}} = \frac{\Delta\text{Cycles}}{200}$$
6. **Physical Distance Conversion**:
   $$\text{Distance (cm)} = \text{Echo Duration (µs)} \times \frac{0.0343}{2} = \text{Echo Duration (µs)} \times 0.01715$$
7. **Range Validation**:
   - Rejects glitches $< 100\ \mu\text{s}$.
   - Valid physical range: **$2.0\text{ cm} \le \text{Distance} \le 400.0\text{ cm}$**.

---

## 5. Serial Telemetry Output

### Example: Object at ~25 cm (SUCCESS)
```
=================================
RA6E2 ULTRASONIC DISTANCE SYSTEM 
=================================
Sensor Status : CONNECTED
Distance      : 24.65 cm
Echo Time     : 1437 us
Measurement   : SUCCESS
```

### Example: Sensor Disconnected / Timeout
```
=================================
RA6E2 ULTRASONIC DISTANCE SYSTEM 
=================================
Sensor Status : TIMEOUT
Distance      : INVALID
Echo Time     : 0 us
Measurement   : ECHO RISING TIMEOUT
```

---

## 6. Build Verification & Memory Usage

Built using **GCC Arm Embedded Toolchain 14.2** via Renesas e² studio GNU Make:

```
arm-none-eabi-size --format=berkeley "RA6E2_Ultrasonic_HCSR04.elf"
   text    data     bss     dec     hex filename
   6364     104    2220    8688    21f0 RA6E2_Ultrasonic_HCSR04.elf
```

- **Flash Utilization**: $6,468\text{ bytes} \approx \mathbf{2.5\%}$ of 256 KB Flash ($249.5\text{ KB}$ free).
- **SRAM Utilization**: $2,324\text{ bytes} \approx \mathbf{5.6\%}$ of 40 KB SRAM ($38.6\text{ KB}$ free).
- **Compiler Flags**: `-mcpu=cortex-m33 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16 -O2 -Wall -Wextra -Wfloat-equal -std=c99`
- **Output Artifacts**:
  - `Debug/RA6E2_Ultrasonic_HCSR04.elf`
  - `Debug/RA6E2_Ultrasonic_HCSR04.hex`
  - `Debug/RA6E2_Ultrasonic_HCSR04.srec`
  - `Debug/RA6E2_Ultrasonic_HCSR04.map`

---

## 7. How to Build, Flash, and Test

### Method A: Using Renesas e² studio
1. Open e² studio.
2. Click **File → Import... → General → Existing Projects into Workspace**.
3. Select root directory: `C:\Renesas_Final\RA6E2_Ultrasonic_HCSR04`.
4. Press **Ctrl+B** to build the project.
5. Click **Debug** to flash the target via the onboard J-Link.

### Method B: One-Click Flashing (Command Line)
1. Close any open JLink / PuTTY sessions.
2. Double-click `flash.bat` in `C:\Renesas_Final\RA6E2_Ultrasonic_HCSR04`.
3. J-Link will connect to target `R7FA6E2BB`, erase flash, and program `RA6E2_Ultrasonic_HCSR04.srec`.

### Viewing Serial Output:
- **Option 1 (J-Link RTT Viewer)**:
  Open **J-Link RTT Viewer**, connect via **SWD @ 4000 kHz**, Device: **R7FA6E2BB**, Terminal: **0**. Telemetry prints in real time.
- **Option 2 (Serial Terminal / PuTTY)**:
  Connect USB-to-UART converter to **Arduino D1** (P411 TX) and **GND**. Open PuTTY at **115200 baud, 8 data bits, no parity, 1 stop bit, no flow control**.

---

## 8. Physical Validation Checklist

| Item | Test Condition | Expected Behavior |
|---|---|---|
| 1 | System Boot & Clocks | DWT counter runs at 200 MHz; boot banner prints over UART and RTT. |
| 2 | Algorithmic Self-Test | Mathematical validation test reports `PASSED` (synthetic 1437 us -> 24.65 cm). |
| 3 | Sensor Disconnected | Firmware prints `Sensor Status: TIMEOUT`, `Measurement: ECHO RISING TIMEOUT`, never hangs. |
| 4 | Obstacle at 10 cm | Sensor reports $\approx 10.0\text{ cm}$, Echo Time $\approx 583\ \mu\text{s}$, LED2 stays ON. |
| 5 | Obstacle at 25 cm | Sensor reports $\approx 24.65\text{ cm}$, Echo Time $\approx 1437\ \mu\text{s}$, LED2 stays ON. |
| 6 | Obstacle < 2 cm | Sensor reports `OUT OF RANGE (< 2 cm)`, Distance `INVALID`, LED2 turns OFF. |
| 7 | No Obstacle (Open air) | Sensor reports `TIMEOUT` or `OUT OF RANGE (> 400 cm)`, recovers smoothly next cycle. |
