# RA6E2 MPU6050 Standalone Bring-Up Project

Standalone embedded firmware project for the **Renesas BGK-RA6E2** development board reading raw and calibrated motion data from a single **MPU6050 6-Axis IMU** over I2C and streaming directly to **PuTTY (COM5 @ 115200 8N1)**.

---

## 1. Hardware Pin Connections

Connect the MPU6050 (GY-521) module directly to the **BGK-RA6E2 Arduino Headers**:

| MPU6050 Pin | BGK-RA6E2 Arduino Header | Port / Pin on MCU | Notes |
|---|---|---|---|
| **VCC** | **3.3V** (or **5V**) | Power Header 3.3V | GY-521 includes onboard 3.3V LDO |
| **GND** | **GND** | Power Header GND | Common Ground reference |
| **SCL** | **D4** | `P500` (`BSP_IO_PORT_05_PIN_00`) | I2C Clock (100 kHz) |
| **SDA** | **D5** | `P409` (`BSP_IO_PORT_04_PIN_09`) | I2C Data (Open-Drain, 4.7 kΩ pullup) |
| **AD0** | **GND** | Power Header GND | Sets I2C 7-bit address to **0x68** |
| **INT** | *Not Connected* | — | Not required for polled operation |

> [!NOTE]
> **Why Arduino Pins D4 and D5?**
> The BGK-RA6E2 board routes the onboard USB CDC Virtual COM port (**COM5**) through MCU pin `P101` (SCI0 TX). By locating the I2C bus on **Arduino D4 (SCL)** and **Arduino D5 (SDA)**, the UART serial line to your laptop over the USB cable remains 100% clean and isolated without electrical bus conflicts.

---

## 2. PuTTY Serial Terminal Settings

Open **PuTTY** and configure:

* **Connection type**: `Serial`
* **Serial line**: `COM5` *(or your board's CDC UART COM port)*
* **Speed (Baud rate)**: `115200`
* **Data bits**: `8`
* **Stop bits**: `1`
* **Parity**: `None`
* **Flow control**: `None`

---

## 3. Expected PuTTY Output

### Startup Banner:
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

### Continuous Telemetry Stream (Every 200 ms):
```
-------------------------------------
ACCELEROMETER
AX: 1234  AY: -2345  AZ: 16000
AX: 0.08 g  AY: -0.14 g  AZ: 0.98 g

GYROSCOPE
GX: 120  GY: -85  GZ: 35
GX: 0.92 dps  GY: -0.65 dps  GZ: 0.27 dps

TEMPERATURE: 25.4 C
-------------------------------------
```

---

## 4. How to Flash the Firmware

1. Plug your BGK-RA6E2 development board into your laptop using the micro-USB cable.
2. Close PuTTY temporarily (so the COM port is released during flashing).
3. Double-click `flash.bat` in this folder, or execute:
   ```cmd
   "C:\Program Files\SEGGER\JLink_V976a\JLink.exe" -CommanderScript "C:\Renesas_Final\RA6E2_MPU6050_Standalone\flash.jlink"
   ```
4. Reopen PuTTY on **COM5 @ 115200** to observe the real-time sensor stream.

---

## 5. Technical Specifications

- **MCU**: Renesas RA6E2 (Arm Cortex-M33 @ 200 MHz, `R7FA6E2BB3CFM`)
- **I2C Clock**: Standard 100 kHz with clock-stretching and 9-pulse bus recovery
- **Accelerometer Range**: $\pm 2g$ ($16384\text{ LSB}/g$)
- **Gyroscope Range**: $\pm 250^\circ/\text{s}$ ($131.0\text{ LSB}/(^\circ/\text{s})$)
- **Temperature Scale**: $T_C = (\text{raw} / 340.0) + 36.53^\circ\text{C}$
- **UART Channels Active**:
  1. Hardware SCI0 TX (`P101`) $\rightarrow$ Onboard USB Virtual COM Port (COM5)
  2. Arduino D1 (`P411`) $\rightarrow$ External 3.3V TTL Serial
  3. SEGGER J-Link RTT $\rightarrow$ Real-Time Terminal 0
