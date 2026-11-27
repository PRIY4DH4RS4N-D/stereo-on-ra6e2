import serial
import serial.tools.list_ports
import time
import sys

def find_jlink_port():
    for p in serial.tools.list_ports.comports():
        if "1366:1024" in p.hwid or "JLink" in p.description or "J-Link" in p.description:
            return p.device
    for p in serial.tools.list_ports.comports():
        return p.device
    return "COM20"

port = find_jlink_port()
print("=" * 60)
print(f" SPINESENSE DUAL MPU6050 SERIAL MONITOR")
print(f" Port: {port} | Baud: 115200 8N1")
print("=" * 60)

try:
    s = serial.Serial(port, 115200, timeout=1.0)
    s.reset_input_buffer()
    while True:
        line = s.readline()
        if line:
            text = line.decode('latin1', errors='replace').rstrip()
            print(text)
            sys.stdout.flush()
except KeyboardInterrupt:
    print("\nExiting serial monitor.")
except Exception as e:
    print(f"\nSerial Error on {port}: {e}")
