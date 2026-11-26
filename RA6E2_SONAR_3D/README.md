# RA6E2 SONAR-3D — Manual 3D Ultrasonic Point Cloud Scanner

A complete, build-verified 3D ultrasonic acoustic mapping and point-cloud scanning system using the **Renesas BGK-RA6E2** development board (Arm Cortex-M33 @ 200 MHz) and a single **HC-SR04** ultrasonic sensor paired with a high-performance **Python PySide6 / PyQtGraph OpenGL** desktop application.

---

## 1. System Overview & Concept

Unlike expensive LiDAR or multi-zone ToF sensors, this project demonstrates high-resolution 3D acoustic spatial scanning using a single low-cost HC-SR04 ultrasonic transducer through **Manual Angular Scanning**:
- The user physically points and tilts the sensor while the host desktop software manages spherical-to-Cartesian coordinate mapping:
  $$\begin{cases}
  X = r \cos(\phi) \sin(\theta) & \text{(Left / Right in mm)} \\
  Y = r \sin(\phi) & \text{(Elevation Up / Down in mm)} \\
  Z = r \cos(\phi) \cos(\theta) & \text{(Depth Forward / Backward in mm)}
  \end{cases}$$
- The host application renders an interactive 3D OpenGL point cloud in real time with depth color mapping (Turbo/Viridis gradient), ground reference grid, Cartesian axes, and aiming vector.
- Includes **Mode A (Manual Point Capture)** and **Mode B (Guided Grid Scanning)** with visual operator feedback.
- Includes an **Offline 3D Acoustic Simulator** with realistic ray-tracing, beam divergence ($15^\circ$), and specular dropouts for environments like flat walls, inside corners, pillars, and rooms.

---

## 2. Hardware Connections & Voltage Divider

> [!CAUTION]
> **ECHO 5V Level Warning**: The HC-SR04 requires 5V VCC and outputs a **5V logic pulse on ECHO**. The RA6E2 MCU GPIOs tolerate a maximum of **3.3V**. Connecting ECHO directly to RA6E2 GPIO will damage the MCU. You **MUST** use a voltage divider.

### Wiring Table

| HC-SR04 Pin | Renesas BGK-RA6E2 Pin | Signal / Header | Description |
| :--- | :--- | :--- | :--- |
| **VCC** | **5V Pin** | 5V Power Header | Supplies 5V to HC-SR04 |
| **GND** | **GND Pin** | Common GND | Ground reference |
| **TRIG** | **P105** | Arduino Header **D2** | 3.3V GPIO Output (10 µs trigger pulse) |
| **ECHO** | **P408** (via divider) | Arduino Header **D3** | 3.3V Safe GPIO Input (Echo pulse width) |
| **UART TX** | **P101** / **P411** | PMOD2 / Arduino **D1** | J-Link CDC UART (Virtual COM @ 115200 8N1) |
| **UART RX** | **P100** | PMOD2 | J-Link CDC UART RX |
| **LED1** | **P104** | Onboard LED1 (Red) | Trigger Activity Indicator |
| **LED2** | **P112** | Onboard LED2 (Green) | Valid Echo Indicator |

### Voltage Divider Circuit
$$\text{HC-SR04 ECHO (5V)} \longrightarrow [R_1 = 1\text{ k}\Omega] \longrightarrow \text{RA6E2 D3 (P408)} \longrightarrow [R_2 = 2\text{ k}\Omega] \longrightarrow \text{GND}$$
$$V_{\text{GPIO}} = 5\text{V} \times \frac{2\text{ k}\Omega}{1\text{ k}\Omega + 2\text{ k}\Omega} = 3.33\text{V (Safe)}$$

---

## 3. Communication Protocol (Line-Based JSON)

Serial settings: **115200 baud, 8 data bits, no parity, 1 stop bit (8N1)**.

### Commands (Host to Board)
- **Measure Distance**:
  ```json
  {"cmd":"MEASURE","request_id":101}
  ```
- **Heartbeat Ping**:
  ```json
  {"cmd":"PING"}
  ```
- **Reset Subsystem**:
  ```json
  {"cmd":"RESET"}
  ```

### Responses (Board to Host)
- **Valid Measurement**:
  ```json
  {"type":"measurement","request_id":101,"distance_mm":425,"echo_us":2478,"status":"ok"}
  ```
- **Echo Timeout (No Reflection / Disconnected)**:
  ```json
  {"type":"measurement","request_id":102,"distance_mm":-1,"echo_us":0,"status":"timeout"}
  ```
- **Out of Range ($< 2\text{ cm}$ or $> 400\text{ cm}$)**:
  ```json
  {"type":"measurement","request_id":103,"distance_mm":-1,"echo_us":28000,"status":"out_of_range"}
  ```
- **Ping Response**:
  ```json
  {"type":"pong","status":"ready"}
  ```

---

## 4. Software Architecture

```
RA6E2_SONAR_3D/
├── firmware/                        # Renesas e² studio FSP Firmware
│   ├── src/
│   │   ├── hal_entry.c              # Application main loop & command dispatcher
│   │   ├── ultrasonic_sensor.c/h    # HC-SR04 driver (direct port register trigger)
│   │   ├── timer_measurement.c/h    # 200 MHz Cortex-M33 DWT cycle counter (5 ns)
│   │   └── uart_protocol.c/h        # Line-based JSON protocol engine
│   ├── ra_gen/                      # FSP generated pin and clock definitions
│   ├── Debug/                       # GNU Arm GCC build target & ELF/HEX outputs
│   ├── build.bat                    # 1-click GCC firmware compilation
│   └── flash.bat / flash.jlink      # J-Link Commander flashing script
│
├── dashboard/                       # Python Host 3D Desktop Application
│   ├── core/
│   │   ├── coordinate_transform.py  # Spherical-to-Cartesian transformation
│   │   ├── protocol.py              # JSON protocol encoder/decoder
│   │   ├── serial_manager.py        # Background QThread serial manager
│   │   ├── pointcloud_processor.py  # Noise filters (EMA, Median, Outliers)
│   │   ├── scan_controller.py       # Mode A / Mode B scanning coordinator
│   │   └── data_manager.py          # CSV, PLY, and JSON export/import
│   ├── visualization/
│   │   └── pointcloud_viewer.py     # PyQtGraph OpenGL 3D viewport
│   ├── ui/
│   │   ├── main_window.py           # PySide6 master window with dark theme
│   │   ├── scanner_panel.py         # Orientation controls & grid guidance
│   │   ├── telemetry_panel.py       # Live digital measurements & statistics
│   │   └── settings_panel.py        # Port, baud, filter sliders, export dialogs
│   ├── simulation/
│   │   └── acoustic_sim.py          # 3D acoustic ray-tracing simulator
│   ├── tests/                       # 18 Automated Unit Tests (pytest)
│   └── main.py                      # Application launcher
│
├── run_dashboard.bat                # 1-click Windows dashboard launcher
├── test_suite.bat                   # 1-click unit test verification
└── README.md                        # Complete project manual
```

---

## 5. Quick Start Guide

### Step 1: Flash Firmware
1. Connect your Renesas BGK-RA6E2 board to the PC via USB.
2. Open terminal in `RA6E2_SONAR_3D/firmware` and run:
   ```cmd
   flash.bat
   ```
   *(Or double-click `flash.bat` in File Explorer).*

### Step 2: Launch Dashboard
Double-click `run_dashboard.bat` or run:
```cmd
python dashboard\main.py
```

### Step 3: Connect or Run Simulation
- **Hardware Mode**: Select `COM5` (or your board's CDC UART port) in **Hardware & Settings** and click **CONNECT**.
- **Offline Simulation Mode**: Check **"Enable Acoustic Simulator"** in **Hardware & Settings** to test scanning without any physical hardware!

### Step 4: Scan the Environment
- **Mode A (Manual Point Capture)**:
  1. Adjust Azimuth slider ($0^\circ$ to $360^\circ$) and Elevation slider ($-45^\circ$ to $+45^\circ$).
  2. Physically point the sensor in that direction.
  3. Press **CAPTURE POINT** (or press `Spacebar`).
  4. The 3D point appears in the OpenGL viewport with depth color!
- **Mode B (Guided Grid Scanning)**:
  1. Configure Azimuth and Elevation bounds and step size.
  2. Click **START GRID SCAN**.
  3. The display will instruct: `Target [1/N]: Align Sensor to Azimuth: -30.0° | Elevation: 0.0°`.
  4. Physically align the sensor and click **CONFIRM SENSOR POSITION** (or press `Spacebar`).

### Step 5: Export Point Cloud
In **Hardware & Settings**, click:
- **Export PLY**: Standard Stanford 3D point cloud format (open directly in MeshLab, CloudCompare, Blender).
- **Export CSV**: Tabular records with timestamps, angles, distances, and $X, Y, Z$ coordinates.
- **Export JSON Session**: Full session backup for future re-inspection.

---

## 6. Build & Test Verification

- **Firmware Compilation**: Built with GCC Arm Embedded Toolchain (`arm-none-eabi-gcc 10.3-2021.10`).
  - Text: 8100 bytes, Data: 184 bytes, BSS: 2664 bytes (Flash: ~8.3 KB / 256 KB; SRAM: ~2.8 KB / 40 KB).
  - Status: **0 Errors, 0 Warnings**.
- **Automated Host Test Suite**: 18 unit tests verified via `pytest`:
  - `test_coordinates.py`: 5 passed
  - `test_protocol.py`: 6 passed
  - `test_pointcloud.py`: 4 passed
  - `test_simulation.py`: 3 passed
  - Total: **18 passed in 0.27s**.
