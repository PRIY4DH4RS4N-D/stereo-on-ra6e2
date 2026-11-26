"""
protocol.py
===========
Line-based JSON Communication Protocol for RA6E2 SONAR-3D.

Encodes outgoing commands from host to board:
  {"cmd": "MEASURE", "request_id": 101}
  {"cmd": "PING"}
  {"cmd": "RESET"}

Parses and validates incoming responses from board:
  {"type": "measurement", "request_id": 101, "distance_mm": 425, "echo_us": 2478, "status": "ok"}
  {"type": "measurement", "request_id": 102, "distance_mm": -1, "echo_us": 0, "status": "timeout"}
  {"type": "pong", "status": "ready"}
"""

import json
from dataclasses import dataclass
from typing import Optional, Dict, Any


@dataclass
class MeasurementResponse:
    request_id: int
    distance_mm: int
    echo_us: int
    status: str
    raw_json: str

    @property
    def is_valid(self) -> bool:
        return self.status == "ok" and 20 <= self.distance_mm <= 4000


class ProtocolCodec:
    """
    Encodes commands to JSON lines and decodes incoming serial responses.
    """

    def __init__(self):
        self._next_request_id: int = 1

    def get_next_request_id(self) -> int:
        req_id = self._next_request_id
        self._next_request_id = (self._next_request_id + 1) % 1000000
        if self._next_request_id == 0:
            self._next_request_id = 1
        return req_id

    def encode_measure(self, request_id: Optional[int] = None) -> bytes:
        if request_id is None:
            request_id = self.get_next_request_id()
        msg = {"cmd": "MEASURE", "request_id": request_id}
        return (json.dumps(msg, separators=(",", ":")) + "\n").encode("utf-8")

    def encode_ping(self) -> bytes:
        msg = {"cmd": "PING"}
        return (json.dumps(msg, separators=(",", ":")) + "\n").encode("utf-8")

    def encode_reset(self) -> bytes:
        msg = {"cmd": "RESET"}
        return (json.dumps(msg, separators=(",", ":")) + "\n").encode("utf-8")

    @staticmethod
    def decode_line(line: str) -> Optional[Dict[str, Any]]:
        """
        Parses a single JSON line string into a dictionary.
        Returns None if parsing fails.
        """
        line = line.strip()
        if not line:
            return None

        # Handle possible leading garbage or framing artifacts
        brace_start = line.find("{")
        brace_end = line.rfind("}")
        if brace_start == -1 or brace_end == -1 or brace_end <= brace_start:
            return None

        clean_json = line[brace_start:brace_end + 1]

        try:
            data = json.loads(clean_json)
            if isinstance(data, dict):
                return data
        except json.JSONDecodeError:
            return None

        return None

    @classmethod
    def parse_measurement(cls, line: str) -> Optional[MeasurementResponse]:
        """
        Parses a line specifically as a MeasurementResponse.
        """
        data = cls.decode_line(line)
        if not data:
            return None

        if data.get("type") == "measurement":
            return MeasurementResponse(
                request_id=int(data.get("request_id", 0)),
                distance_mm=int(data.get("distance_mm", -1)),
                echo_us=int(data.get("echo_us", 0)),
                status=str(data.get("status", "unknown")),
                raw_json=line.strip()
            )

        return None
