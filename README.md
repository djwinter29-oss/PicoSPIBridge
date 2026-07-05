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
- DMA to move captured bytes out of the PIO RX FIFO in fixed blocks
- A software ring buffer to decouple capture from USB transmission
- TinyUSB CDC to stream binary data to the host PC

The current scaffold uses 64-byte DMA blocks to keep IRQ load low enough for at least 5 MHz SPI capture, flushes partial blocks when chip select releases so short transfers still reach the CDC stream, and keeps a larger ring buffer so USB startup has more headroom before traffic would be dropped.

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
2. Move incoming data into a DMA/ring buffer path
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

The ring buffer has a small host-side unit test so you can validate the core queue logic without the Pico SDK toolchain.

1. Configure the test build:

	```powershell
	cmake -S firmware/tests -B build/tests
	```

2. Build the test:

	```powershell
	cmake --build build/tests
	```

3. Run the test:

	```powershell
	ctest --test-dir build/tests --output-on-failure
	```

## Tool Scripts

Helper scripts are provided under `tools/` for both Windows and Linux:

- `tools/windows/build.ps1` configures and builds firmware plus host tests
- `tools/windows/test.ps1` builds tests if needed and runs `ctest`
- `tools/windows/coverage.ps1` builds host tests with coverage instrumentation and writes reports under `build/coverage`
- `tools/linux/build.sh` configures and builds firmware plus host tests
- `tools/linux/test.sh` builds tests if needed and runs `ctest`
- `tools/linux/coverage.sh` builds host tests with coverage instrumentation and writes reports under `build/coverage`

Coverage scripts require `gcovr` and a GCC- or Clang-based host compiler.

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
- The current DMA path is sized for a minimum target of 5 MHz SPI monitoring, assuming the host keeps up with the USB CDC stream
- The current ring buffer is sized to provide about 100 ms of capture headroom at 5 MHz SPI before USB backpressure would force drops
- `GPIO4` is reserved for possible future MISO monitoring, but MISO capture is not supported right now

## Status

This repository contains a first-pass Pico SDK firmware scaffold under `firmware/` for MOSI-only streaming. It is intended as a minimal base that can be refined against the target SPI timing and framing requirements.
