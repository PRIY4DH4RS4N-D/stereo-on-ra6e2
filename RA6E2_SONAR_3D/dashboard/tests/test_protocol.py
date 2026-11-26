"""
test_protocol.py
================
Unit tests for line-based JSON protocol serialization and decoding.
"""

import json
import pytest
from dashboard.core.protocol import ProtocolCodec, MeasurementResponse


def test_encode_measure():
    codec = ProtocolCodec()
    payload = codec.encode_measure(request_id=101)
    assert payload.endswith(b"\n")

    decoded = json.loads(payload.decode("utf-8").strip())
    assert decoded["cmd"] == "MEASURE"
    assert decoded["request_id"] == 101


def test_encode_ping():
    codec = ProtocolCodec()
    payload = codec.encode_ping()
    decoded = json.loads(payload.decode("utf-8").strip())
    assert decoded["cmd"] == "PING"


def test_decode_valid_measurement():
    line = '{"type":"measurement","request_id":101,"distance_mm":425,"echo_us":2478,"status":"ok"}\r\n'
    resp = ProtocolCodec.parse_measurement(line)

    assert resp is not None
    assert resp.request_id == 101
    assert resp.distance_mm == 425
    assert resp.echo_us == 2478
    assert resp.status == "ok"
    assert resp.is_valid is True


def test_decode_timeout_measurement():
    line = '{"type":"measurement","request_id":102,"distance_mm":-1,"echo_us":0,"status":"timeout"}'
    resp = ProtocolCodec.parse_measurement(line)

    assert resp is not None
    assert resp.request_id == 102
    assert resp.distance_mm == -1
    assert resp.status == "timeout"
    assert resp.is_valid is False


def test_decode_dirty_framing():
    # Message surrounded by noise or extra garbage
    dirty = '>>>GARBAGE<<<{"type":"measurement","request_id":205,"distance_mm":800,"echo_us":4664,"status":"ok"}***NOISE'
    resp = ProtocolCodec.parse_measurement(dirty)

    assert resp is not None
    assert resp.request_id == 205
    assert resp.distance_mm == 800
    assert resp.is_valid is True


def test_decode_malformed_json():
    bad_line = '{"type":"measurement","request_id":BAD,}'
    assert ProtocolCodec.decode_line(bad_line) is None
    assert ProtocolCodec.parse_measurement(bad_line) is None
