# SpineSense_3IMU — MPU6050 UART Hardware Bring-up Test

Hardware communication and verification test for a single **MPU6050 6-Axis MotionTracking IMU** (3-axis Accelerometer + 3-axis Gyroscope + Temperature) interfaced to the **Renesas BGK-RA6E2** microcontroller (Arm Cortex-M33 @ 200 MHz).

---

## 1. Hardware Connections

The Renesas BGK-RA6E2 board exposes the I2C bus on the standard **Arduino Header** pins (P100 / P101).

### Wiring Table

| MPU6050 (GY-521) Pin | Renesas BGK-RA6E2 Pin | Header Location | Description |
| :--- | :--- | :--- | :--- |
| **VCC** | **3.3V** (or **5V**) | Power Header | Power supply (GY-521 modules include an onboard 3.3V LDO) |
| **GND** | **GND** | Power Header | Common Ground |
| **SCL** | **P100** | Arduino Header **SCL** (PMOD2 Pin 4) | I2C Clock Line (Open-Drain, pulled up to 3.3V) |
| **SDA** | **P101** | Arduino Header **SDA** (PMOD2 Pin 3) | I2C Data Line (Open-Drain, pulled up to 3.3V) |
| **AD0** | **GND** | GND | Sets I2C 7-bit slave address to **0x68** |
| **INT** | *Not Connected* | — | Unused for polled bring-up test |

> [!NOTE]
> **Voltage Compatibility**: Standard GY-521 breakout modules contain an onboard 3.3V low-dropout voltage regulator (e.g. 662K/KB33) and $4.7\text{ k}\Omega$ pull-up resistors to 3.3V on both SCL and SDA. You can safely power the module from either the **3.3V** or **5V** header on the BGK-RA6E2 board.

---

## 2. Firmware Register Architecture

The driver interacts directly with the standard MPU6050 registers:

| Register Name | Address | Configured Value | Purpose |
| :--- | :--- | :--- | :--- |
| **WHO_AM_I** | `0x75` | `0x68` (read-only) | Verification of sensor identity |
| **PWR_MGMT_1** | `0x6B` | `0x00` | Clears SLEEP bit (bit 6) to wake up sensor |
| **ACCEL_CONFIG**| `0x1C` | `0x00` | Selects ±2g full scale range ($16,384\text{ LSB/g}$) |
| **GYRO_CONFIG** | `0x1B` | `0x00` | Selects ±250 °/s full scale range ($131.0\text{ LSB/(°/s)}$) |
| **ACCEL_XOUT_H**| `0x3B` | *Read* | Start of 14-byte burst read for Accel, Temp, and Gyro |

### Engineering Unit Conversion Formulas
- **Accelerometer ($\pm 2g$)**:
  $$g = \frac{\text{raw\_accel}}{16384.0}$$
- **Gyroscope ($\pm 250^\circ/\text{s}$)**:
  $$\text{dps} = \frac{\text{raw\_gyro}}{131.0}$$
- **Temperature ($^\circ\text{C}$)**:
  $$T_C = \frac{\text{raw\_temp}}{340.0} + 36.53$$

---

## 3. Serial Terminal Output

Serial Configuration: **115200 Baud, 8 Data Bits, No Parity, 1 Stop Bit (8N1)**.

### Startup Screen (When MPU6050 is connected)
```
=====================================
       SPINESENSE - MPU6050 TEST     
=====================================
RA6E2 I2C Initialization: OK
UART Initialization: OK
MPU6050 Detection: SUCCESS
WHO_AM_I: 0x68
Sensor Configuration: OK

Starting continuous sensor readings (200 ms interval)...
```

### Continuous Readings (Every 200 ms)
```
-------------------------------------
ACCELEROMETER
AX: 184  AY: -412  AZ: 16280
AX: 0.01 g  AY: -0.03 g  AZ: 0.99 g

GYROSCOPE
GX: -32  GY: 14  GZ: -8
GX: -0.24 dps  GY: 0.11 dps  GZ: -0.06 dps

TEMPERATURE: 26.8 C
-------------------------------------
```

### Disconnection / Detection Failure Output
If the sensor is disconnected, the firmware gracefully informs the user without freezing or printing fabricated numbers:
```
-------------------------------------
[WARNING] MPU6050 Not Detected at I2C Address 0x68!
  -> Check Wiring: VCC=3.3V/5V, GND=GND, SCL=P100, SDA=P101, AD0=GND
  -> Retrying detection in 1 second...
-------------------------------------
```

---

## 4. How to Build & Flash

### 1-Click Build (GCC Arm Embedded)
Run [build.bat](file:///C:/Renesas_Final/SpineSense_3IMU/build.bat) or execute:
```cmd
C:\Renesas_Final\SpineSense_3IMU\build.bat
```
- Compiler: `arm-none-eabi-gcc 10.3-2021.10`
- Output: `SpineSense_3IMU.elf`, `SpineSense_3IMU.hex`, `SpineSense_3IMU.srec`
- Resource Usage: **7.3 KB Flash** (~2.8%), **2.3 KB SRAM** (~5.7%). Zero errors.

### 1-Click Flash (SEGGER J-Link)
Run [flash.bat](file:///C:/Renesas_Final/SpineSense_3IMU/flash.bat) or execute:
```cmd
C:\Renesas_Final\SpineSense_3IMU\flash.bat
```

---

## 5. Viewing the Output

You can monitor the output using either of the following channels:

1. **J-Link CDC UART (Virtual COM port)**:
   - Connect PuTTY, Tera Term, or Serial Terminal to your board's COM port (e.g. `COM20`) at **115200 8N1**.
2. **External USB-to-UART TTL Adapter**:
   - Connect your USB-TTL adapter's RX pin to Arduino Header **D1** (`P411`) and GND to common GND. Open terminal at **115200 8N1**.
3. **SEGGER J-Link RTT Viewer**:
   - Open **J-Link RTT Viewer**, connect to target `R7FA6E2BB` via SWD at 4000 kHz, select **Terminal 0**. Real-time logs appear instantly!
