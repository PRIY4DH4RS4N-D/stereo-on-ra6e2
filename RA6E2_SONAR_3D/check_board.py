import serial
import time

print("Checking COM5...")
try:
    s = serial.Serial('COM5', 115200, timeout=1.0)
    time.sleep(0.2)
    incoming = s.read(s.in_waiting) if s.in_waiting else b""
    print("Incoming on connection:", repr(incoming))
    
    print("Sending PING...")
    s.write(b'{"cmd":"PING"}\r\n')
    time.sleep(0.3)
    resp = s.read(s.in_waiting) if s.in_waiting else s.read(100)
    print("PING response:", repr(resp))

    print("Sending MEASURE...")
    s.write(b'{"cmd":"MEASURE","request_id":1}\r\n')
    time.sleep(0.5)
    resp_m = s.read(s.in_waiting) if s.in_waiting else s.read(100)
    print("MEASURE response:", repr(resp_m))
    s.close()
except Exception as e:
    print("Exception on COM5:", e)
