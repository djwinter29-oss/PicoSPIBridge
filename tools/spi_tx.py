#!/usr/bin/env python3
"""Send one or more fixed-size payloads out over Raspberry Pi spi0.0.

Examples:
  python tools/spi_tx.py --size-mb 1 --speed-mhz 8
  python tools/spi_tx.py --size-mb 5 --speed-mhz 20 --chunk-bytes 4096
  python tools/spi_tx.py --sizes-mb 1,2,3,4,5 --speed-mhz 12 

Requirements:
  - Raspberry Pi with SPI enabled
  - python3-spidev installed

The transmitted byte pattern is fixed and known: byte n is n modulo 256.
"""

from __future__ import annotations

import argparse
import importlib
import json
import time
from dataclasses import asdict, dataclass


BYTES_PER_MB = 1_000_000
DEFAULT_CHUNK_BYTES = 4096
VALID_SIZES_MB = (1, 2, 3, 4, 5)


@dataclass
class TransferResult:
    bus: int
    device: int
    size_mb: int
    bytes_sent: int
    speed_mhz: float
    elapsed_s: float
    bytes_per_s: float
    mib_per_s: float


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bus", type=int, default=0, help="SPI bus number. Defaults to 0.")
    parser.add_argument("--device", type=int, default=0, help="SPI chip-select/device number. Defaults to 0.")
    size_group = parser.add_mutually_exclusive_group(required=True)
    size_group.add_argument(
        "--size-mb",
        type=int,
        choices=VALID_SIZES_MB,
        help="Single payload size in MB. Allowed values: 1, 2, 3, 4, 5.",
    )
    size_group.add_argument(
        "--sizes-mb",
        help="Comma-separated payload sizes in MB. Allowed values: 1,2,3,4,5.",
    )
    parser.add_argument(
        "--speed-mhz",
        type=float,
        required=True,
        help="Requested SPI clock in MHz.",
    )
    parser.add_argument(
        "--chunk-bytes",
        type=int,
        default=DEFAULT_CHUNK_BYTES,
        help="Bytes written per transfer chunk. Defaults to 4096.",
    )
    return parser.parse_args()


def build_chunk(chunk_bytes: int, start_offset: int) -> bytearray:
    if chunk_bytes <= 0:
        raise ValueError("chunk_bytes must be greater than zero")

    return bytearray((start_offset + index) & 0xFF for index in range(chunk_bytes))


def open_spi(bus: int, device: int, speed_mhz: float):
    try:
        spidev = importlib.import_module("spidev")
    except ImportError as exc:
        raise SystemExit(
            "python3-spidev is required on the Raspberry Pi. Install it with 'sudo apt install python3-spidev'."
        ) from exc

    spi = spidev.SpiDev()
    spi.open(bus, device)
    spi.mode = 0
    spi.bits_per_word = 8
    spi.max_speed_hz = int(speed_mhz * 1_000_000)
    return spi


def run_transfer(
    *,
    bus: int,
    device: int,
    size_mb: int,
    speed_mhz: float,
    chunk_bytes: int,
) -> TransferResult:
    total_bytes = size_mb * BYTES_PER_MB
    spi = open_spi(bus, device, speed_mhz)

    bytes_sent = 0
    start = time.monotonic()
    try:
        while bytes_sent < total_bytes:
            remaining = total_bytes - bytes_sent
            transfer_size = min(chunk_bytes, remaining)
            spi.writebytes2(build_chunk(transfer_size, bytes_sent))
            bytes_sent += transfer_size
    finally:
        spi.close()

    elapsed_s = time.monotonic() - start
    return TransferResult(
        bus=bus,
        device=device,
        size_mb=size_mb,
        bytes_sent=bytes_sent,
        speed_mhz=speed_mhz,
        elapsed_s=elapsed_s,
        bytes_per_s=bytes_sent / elapsed_s if elapsed_s > 0 else 0.0,
        mib_per_s=(bytes_sent / (1024 * 1024)) / elapsed_s if elapsed_s > 0 else 0.0,
    )


def parse_sizes(size_mb: int | None, sizes_mb: str | None) -> list[int]:
    if size_mb is not None:
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


def main() -> int:
    args = parse_args()
    try:
        sizes = parse_sizes(args.size_mb, args.sizes_mb)
    except ValueError as exc:
        raise SystemExit(str(exc)) from exc

    results = []
    for size_mb in sizes:
        result = run_transfer(
            bus=args.bus,
            device=args.device,
            size_mb=size_mb,
            speed_mhz=args.speed_mhz,
            chunk_bytes=args.chunk_bytes,
        )
        payload = asdict(result)
        payload["elapsed_s"] = round(result.elapsed_s, 6)
        payload["bytes_per_s"] = round(result.bytes_per_s, 1)
        payload["mib_per_s"] = round(result.mib_per_s, 3)
        results.append(payload)
        print(json.dumps(payload))

    if len(results) > 1:
        print(json.dumps({"runs": results}))

    return 0


if __name__ == "__main__":
    raise SystemExit(main())