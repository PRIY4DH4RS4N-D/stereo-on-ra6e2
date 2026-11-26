import serial
import time

try:
    s = serial.Serial('COM5', 115200, timeout=1.0)
    time.sleep(0.2)
    s.reset_input_buffer()
    s.write(b'{"cmd":"PING"}\r\n')
    time.sleep(0.2)
    data = s.read(s.in_waiting or 100)
    print("Received:", repr(data))
    
    s.write(b'{"cmd":"MEASURE","request_id":101}\r\n')
    time.sleep(0.3)
    data2 = s.read(s.in_waiting or 100)
    print("Measure response:", repr(data2))
    s.close()
except Exception as e:
    print("Serial error:", e)
