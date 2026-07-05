# PicoSPIBridge Pin Definitions

This project uses the following RP2040 GPIO assignments for the observed SPI bus:

| GPIO | Signal | Description |
| --- | --- | --- |
| `GPIO2` | `SPI0_SCLK` | SPI bus 0 clock input |
| `GPIO3` | `SPI0_MOSI` | SPI bus 0 controller-to-peripheral data input |
| `GPIO4` | `SPI0_MISO` | Reserved SPI bus 0 peripheral-to-controller data input, not currently supported |
| `GPIO5` | `SPI0_CS_N0` | First observed transaction boundary input on SPI bus 0 |

Notes:

- The current firmware monitors MOSI traffic only.
- `GPIO4` is reserved as a future MISO observation pin, but MISO capture is not supported by the current firmware.
- `GPIO5` is used as the active-low chip select boundary for deciding when MOSI bytes belong to a valid transaction.