# PicoSPIBridge

PicoSPIBridge uses the RP2040 as a low-cost SPI MOSI monitor that forwards captured traffic to a PC over USB CDC serial.

The design is intentionally simple:

`SPI MOSI monitor -> DMA/ring buffer -> USB CDC binary stream -> PC application`

This project is intended for passive MOSI-only SPI traffic observation, with the RP2040 acting as a bridge between the target bus and a host-side application.

## Firmware Project

This repository now includes a minimal RP2040 C firmware project under `firmware/`, built with the Pico SDK.

The implementation uses:

- PIO to sample MOSI on SCK rising edges
- Active-low chip select gating so capture starts only during transfers
- Chained ping-pong DMA targets that write directly into reserved ring-buffer spans
- A software ring buffer to decouple capture from USB transmission and absorb USB backpressure
- TinyUSB CDC to stream binary data to the host PC with short-packet-aware flush behavior

The current scaffold uses chained ping-pong DMA reservations of up to 4 KB each to reduce IRQ overhead, flushes partial blocks when chip select releases so short transfers still reach the CDC stream without aborting the active DMA transfer, falls back to scratch overflow buffers only when the ring has no free reservation space, and keeps lightweight runtime counters for USB writes, USB flushes, overflow-buffer commits, DMA rearms, and ring high-water mark.

## Overview

The firmware monitors SPI MOSI traffic, stores captured bytes in a DMA-backed ring buffer, and streams the data out through the RP2040 USB CDC interface.

Key behavior:

- Starts bridging MOSI traffic immediately at startup
- No CLI or runtime command interface
- USB CDC is read-only from the PC side
- Focused on keeping the firmware path as small and direct as possible

## Data Flow

The capture path is:

1. Monitor SPI MOSI traffic on the RP2040
2. DMA incoming data directly into reserved ring-buffer spans
3. Stream the captured bytes over USB CDC as a binary data stream
4. Let a PC application decode, log, or display the traffic

In the current firmware scaffold, the RP2040 listens on:

- `GPIO2` for SPI clock
- `GPIO3` for SPI MOSI
- `GPIO4` reserved for a future SPI MISO observation path and not supported by the current firmware
- `GPIO5` for active-low chip select

## Project Goals

- Use inexpensive RP2040 hardware as a MOSI-only SPI monitoring bridge
- Keep the firmware architecture simple and deterministic
- Avoid configuration layers that add overhead or complexity
- Provide a binary stream that a host application can process directly

## Operating Model

PicoSPIBridge is designed as an always-on bridge.

When powered up, the firmware begins forwarding monitored traffic without waiting for user commands. There is no shell, command parser, or text protocol exposed over the serial interface.

The USB CDC connection is output-only for captured data. The host should treat the device as a read-only binary stream source.

## Typical Use

1. Connect the RP2040 device to the monitored SPI bus
2. Connect the RP2040 USB port to a PC
3. Open the USB CDC serial device from a host application
4. Read and process the binary output stream

## Build

This project uses the Raspberry Pi Pico SDK.

1. Install the Pico SDK on your machine
2. Set the `PICO_SDK_PATH` environment variable to your Pico SDK checkout
3. Configure the build:

	```powershell
	cmake -S firmware -B build/firmware
	```

4. Build the firmware:

	```powershell
	cmake --build build/firmware
	```

5. Flash the generated UF2 file from `build/firmware` to the RP2040 board

## Unit Tests

The repository includes host-side unit tests for the ring buffer, DMA span planning, overflow accounting, and USB flush behavior. The scripted test flow also builds the real Pico firmware first, so the generated PIO header and target-side capture sources are validated alongside the host logic tests.

1. Configure the test build:

	```powershell
	cmake -S firmware/tests -B build/tests
	```

2. Build the test:

	```powershell
	cmake --build build/tests --config Debug
	```

3. Run the test:

	```powershell
	ctest --test-dir build/tests -C Debug --output-on-failure
	```

On Windows, `tools/windows/test.ps1` is the simplest way to run validation because it bootstraps the Visual Studio developer environment, builds the Pico firmware, and then runs the host tests.

## Tool Scripts

Helper scripts are provided under `tools/` for both Windows and Linux:

- `tools/windows/build.ps1` configures and builds firmware plus host tests
- `tools/windows/test.ps1` builds the firmware, rebuilds the host tests, and runs `ctest`
- `tools/windows/coverage.ps1` builds host tests with coverage instrumentation and writes reports under `build/coverage`
- `tools/linux/build.sh` configures and builds firmware plus host tests
- `tools/linux/test.sh` builds the firmware, rebuilds the host tests, and runs `ctest`
- `tools/linux/coverage.sh` builds host tests with coverage instrumentation and writes reports under `build/coverage`

Coverage scripts require `gcovr` and a GCC- or Clang-based host compiler.

## Project Structure

The repository is intentionally kept small:

- `firmware/` contains the RP2040 firmware project and its Pico SDK build files
- `firmware/src/` contains the main firmware sources
- `firmware/src/capture/` contains the SPI capture implementation, including the PIO program and DMA-driven capture logic
- `firmware/tests/` contains host-side unit tests for logic that can be validated without RP2040 hardware
- `tools/` contains helper scripts for build, test, and coverage on Windows and Linux
- `docs/` contains supporting documentation such as pin assignments

Within `firmware/src/`, the structure is intentionally light:

- `main.c` handles firmware startup and the main loop
- `bridge_ring.*` provides the ring buffer used between capture and USB streaming
- `usb_stream.*` handles forwarding buffered bytes to USB CDC
- `bridge_config.h` keeps shared firmware constants in one place

## Wiring

- Connect target SPI `SCK` to RP2040 `GPIO2`
- Connect target SPI `MOSI` to RP2040 `GPIO3`
- `GPIO4` is reserved for future MISO support and is not used by the current firmware
- Connect target SPI `CS` or `SS` to RP2040 `GPIO5`
- Connect ground between the target system and the RP2040

See `docs/pin-definitions.md` for the pin mapping summary.

## Notes

- This project is aimed at monitoring MOSI traffic only, not controlling the SPI bus
- Host software is expected to understand the binary stream format used by the firmware
- Because the interface starts streaming on boot, host software should be ready to consume data as soon as the device enumerates
- The current scaffold discards partial bytes immediately when active-low chip select deasserts, so only selected-transfer data is forwarded
- The current DMA and USB buffering path is tuned to reduce per-byte CPU overhead while still targeting at least 5 MHz SPI monitoring, assuming the host keeps up with the USB CDC stream
- The current ring buffer is sized to provide about 200 ms of capture headroom at 5 MHz SPI before USB backpressure would force drops
- When the host falls behind badly enough that the ring cannot reserve another DMA span, the firmware keeps capture running and counts the overflow as dropped bytes
- Runtime counters now track USB write calls, USB bytes written, USB flush calls, overflow-buffer commits, DMA rearms, publish-invariant failures, and ring high-water mark for on-target tuning
- Host-side tests cover ring publish semantics, publish-invariant failure handling, USB short-packet flush behavior, overflow-commit accounting, and DMA span reservation math without requiring RP2040 hardware
- `GPIO4` is reserved for possible future MISO monitoring, but MISO capture is not supported right now

## Status

This repository contains a first-pass Pico SDK firmware scaffold under `firmware/` for MOSI-only streaming. It is intended as a minimal base that can be refined against the target SPI timing and framing requirements.
