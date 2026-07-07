#!/usr/bin/env python3
"""Read captured bytes from PicoSPIBridge CDC and validate them against a fixed known pattern.

Examples:
    python tools/cdc_rx.py --port COM11
    python tools/cdc_rx.py --port /dev/ttyACM0 --sizes-mb 1,2,3

Requirements:
  pip install pyserial

The expected byte pattern is fixed and known: byte n is n modulo 256.
"""

from __future__ import annotations

import argparse
import json
import sys
import time
from dataclasses import asdict, dataclass

import serial
from serial.tools import list_ports

VID = 0xCAFE
PID = 0x4001
IDLE_TIMEOUT_S = 0.1
READ_SIZE = 65536
VALID_SIZES_MB = (1, 2, 3, 4, 5)


@dataclass
class Mismatch:
    offset: int
    expected: int
    actual: int


@dataclass
class CaptureResult:
    port: str
    bytes_received: int
    elapsed_s: float
    bytes_per_s: float
    mib_per_s: float
    expected_bytes: int | None
    complete: bool | None
    mismatch: Mismatch | None


def auto_detect_port() -> str | None:
    for port in list_ports.comports():
        if port.vid == VID and port.pid == PID:
            return port.device

    for port in list_ports.comports():
        desc = f"{port.description} {port.manufacturer or ''} {port.product or ''}"
        if "PicoSPIBridge" in desc:
            return port.device

    return None


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="Serial port, e.g. COM11. If omitted, auto-detect by VID/PID.")
    parser.add_argument(
        "--baud",
        type=int,
        default=115200,
        help="Ignored for CDC, kept for convenience. Defaults to 115200.",
    )
    size_group = parser.add_mutually_exclusive_group()
    size_group.add_argument("--size-mb", type=int, help="Expected single payload size in MB.")
    size_group.add_argument("--sizes-mb", help="Expected comma-separated payload sizes in MB.")
    return parser.parse_args()


def parse_sizes(size_mb: int | None, sizes_mb: str | None) -> list[int]:
    if size_mb is not None:
        if size_mb not in VALID_SIZES_MB:
            raise ValueError(f"Unsupported size '{size_mb}'. Choose from {VALID_SIZES_MB}.")
        return [size_mb]

    if sizes_mb is None:
        raise ValueError("One of --size-mb or --sizes-mb is required.")

    sizes: list[int] = []
    for item in sizes_mb.split(","):
        stripped = item.strip()
        if not stripped:
            continue

        size = int(stripped)
        if size not in VALID_SIZES_MB:
            raise ValueError(f"Unsupported size '{size}'. Choose from {VALID_SIZES_MB}.")
        sizes.append(size)

    if not sizes:
        raise ValueError("At least one size must be provided.")

    return sizes


def expected_byte_at(offset: int) -> int:
    if offset < 0:
        raise ValueError("offset must be non-negative")

    return offset & 0xFF


def find_first_mismatch(data: bytes, start_offset: int) -> Mismatch | None:
    for index, value in enumerate(data):
        offset = start_offset + index
        expected = expected_byte_at(offset)
        if value != expected:
            return Mismatch(offset=offset, expected=expected, actual=value)

    return None


def read_capture(
    *,
    port: str,
    baud: int,
    expected_bytes: int | None,
) -> CaptureResult:
    serial_port = serial.Serial(
        port=port,
        baudrate=baud,
        timeout=IDLE_TIMEOUT_S,
        write_timeout=1,
    )

    bytes_received = 0
    first_byte_time: float | None = None
    last_byte_time: float | None = None
    mismatch: Mismatch | None = None

    try:
        try:
            serial_port.reset_input_buffer()
        except Exception:
            pass

        while True:
            chunk = serial_port.read(READ_SIZE)
            now = time.monotonic()
            if not chunk:
                if first_byte_time is None:
                    continue
                if last_byte_time is not None and now - last_byte_time >= IDLE_TIMEOUT_S:
                    break
                continue

            if first_byte_time is None:
                first_byte_time = now
            last_byte_time = now

            if mismatch is None:
                mismatch = find_first_mismatch(chunk, bytes_received)

            bytes_received += len(chunk)

            if expected_bytes is not None and bytes_received >= expected_bytes:
                continue
    finally:
        serial_port.close()

    if first_byte_time is None or last_byte_time is None:
        elapsed_s = 0.0
    else:
        elapsed_s = max(last_byte_time - first_byte_time, 0.0)

    return CaptureResult(
        port=port,
        bytes_received=bytes_received,
        elapsed_s=elapsed_s,
        bytes_per_s=bytes_received / elapsed_s if elapsed_s > 0 else 0.0,
        mib_per_s=(bytes_received / (1024 * 1024)) / elapsed_s if elapsed_s > 0 else 0.0,
        expected_bytes=expected_bytes,
        complete=(bytes_received == expected_bytes) if expected_bytes is not None else None,
        mismatch=mismatch,
    )


def main() -> int:
    args = parse_args()
    port = args.port or auto_detect_port()
    if not port:
        print("No PicoSPIBridge CDC port found. Pass --port COMx.", file=sys.stderr)
        return 2

    expected_bytes = None
    if args.size_mb is not None or args.sizes_mb is not None:
        try:
            sizes = parse_sizes(args.size_mb, args.sizes_mb)
        except ValueError as exc:
            raise SystemExit(str(exc)) from exc
        expected_bytes = sum(sizes) * 1_000_000

    result = read_capture(
        port=port,
        baud=args.baud,
        expected_bytes=expected_bytes,
    )

    payload = asdict(result)
    payload["elapsed_s"] = round(result.elapsed_s, 6)
    payload["bytes_per_s"] = round(result.bytes_per_s, 1)
    payload["mib_per_s"] = round(result.mib_per_s, 3)
    print(json.dumps(payload))
    return 0 if result.mismatch is None else 1


if __name__ == "__main__":
    raise SystemExit(main())