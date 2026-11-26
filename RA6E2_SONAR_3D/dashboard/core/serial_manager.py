"""
serial_manager.py
=================
Threaded serial communication manager for BGK-RA6E2 USB UART interface.
Emits PySide6 signals for UI updates.
"""

import time
import queue
import threading
from typing import Optional, List, Dict, Any

from PySide6.QtCore import QObject, Signal, QThread
import serial
import serial.tools.list_ports

from dashboard.core.protocol import ProtocolCodec, MeasurementResponse


class SerialManager(QObject):
    """
    Manages non-blocking serial communication with the RA6E2 firmware.
    """

    # Signals
    connected = Signal(str)           # port name
    disconnected = Signal()
    error_occurred = Signal(str)      # error message
    measurement_received = Signal(MeasurementResponse)
    pong_received = Signal()
    raw_line_received = Signal(str)

    def __init__(self, default_baud: int = 115200):
        super().__init__()
        self.default_baud = default_baud
        self._serial: Optional[serial.Serial] = None
        self._port_name: Optional[str] = None
        self._is_running = False
        self._codec = ProtocolCodec()

        self._read_thread: Optional[threading.Thread] = None
        self._tx_queue: queue.Queue = queue.Queue()

        # Outstanding requests: request_id -> callback or future
        self._pending_requests: Dict[int, float] = {}  # req_id -> timestamp
        self._lock = threading.Lock()

    @staticmethod
    def list_available_ports() -> List[str]:
        """
        Returns a list of available COM port descriptions.
        """
        ports = serial.tools.list_ports.comports()
        return [p.device for p in ports]

    def is_connected(self) -> bool:
        return self._serial is not None and self._serial.is_open

    def connect_port(self, port: str, baud: int = 115200) -> bool:
        """
        Opens connection to the given COM port.
        """
        self.disconnect_port()

        try:
            self._serial = serial.Serial(
                port=port,
                baudrate=baud,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                timeout=0.1,
                write_timeout=0.5
            )
            self._port_name = port
            self._is_running = True

            # Start background reader thread
            self._read_thread = threading.Thread(target=self._reader_loop, daemon=True)
            self._read_thread.start()

            self.connected.emit(port)
            return True
        except Exception as e:
            self.error_occurred.emit(f"Failed to open {port}: {str(e)}")
            self._serial = None
            return False

    def disconnect_port(self):
        """
        Closes the serial connection.
        """
        self._is_running = False
        if self._read_thread and self._read_thread.is_alive():
            self._read_thread.join(timeout=0.5)

        if self._serial:
            try:
                self._serial.close()
            except Exception:
                pass
            self._serial = None

        self._port_name = None
        self.disconnected.emit()

    def send_measure_request(self) -> int:
        """
        Sends a MEASURE command to the board. Returns the request_id.
        """
        req_id = self._codec.get_next_request_id()
        payload = self._codec.encode_measure(req_id)

        with self._lock:
            self._pending_requests[req_id] = time.time()

        self._write_raw(payload)
        return req_id

    def send_ping(self):
        payload = self._codec.encode_ping()
        self._write_raw(payload)

    def send_reset(self):
        payload = self._codec.encode_reset()
        self._write_raw(payload)

    def _write_raw(self, data: bytes):
        if not self.is_connected():
            return
        try:
            self._serial.write(data)
            self._serial.flush()
        except Exception as e:
            self.error_occurred.emit(f"Serial write error: {str(e)}")

    def _reader_loop(self):
        """
        Background loop reading incoming serial lines.
        """
        buffer = bytearray()
        while self._is_running and self._serial and self._serial.is_open:
            try:
                chunk = self._serial.read(self._serial.in_waiting or 1)
                if not chunk:
                    continue

                buffer.extend(chunk)

                # Process newline-delimited lines
                while b"\n" in buffer:
                    line_bytes, buffer = buffer.split(b"\n", 1)
                    line_str = line_bytes.decode("utf-8", errors="replace").strip()
                    if not line_str:
                        continue

                    self.raw_line_received.emit(line_str)
                    self._process_line(line_str)

            except serial.SerialException as e:
                if self._is_running:
                    self.error_occurred.emit(f"Serial read error: {str(e)}")
                    break
            except Exception as e:
                if self._is_running:
                    self.error_occurred.emit(f"Unexpected reader error: {str(e)}")

        self._is_running = False

    def _process_line(self, line: str):
        """
        Parses JSON lines and dispatches measurements / responses.
        """
        data = self._codec.decode_line(line)
        if not data:
            return

        msg_type = data.get("type")

        if msg_type == "measurement":
            measurement = self._codec.parse_measurement(line)
            if measurement:
                with self._lock:
                    self._pending_requests.pop(measurement.request_id, None)
                self.measurement_received.emit(measurement)

        elif msg_type == "pong":
            self.pong_received.emit()
