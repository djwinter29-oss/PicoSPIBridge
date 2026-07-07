#!/usr/bin/env bash
set -euo pipefail

firmware_build_dir="${1:-build/firmware}"
openocd_exe="${OPENOCD_EXE:-openocd}"
adapter_speed_khz="${PICO_DEBUG_PROBE_SPEED_KHZ:-5000}"

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../.." && pwd)"
elf_path="${repo_root}/${firmware_build_dir}/pico_spi_bridge.elf"

cd "${repo_root}"

cmake -S firmware -B "${firmware_build_dir}"
cmake --build "${firmware_build_dir}"

if ! command -v "${openocd_exe}" >/dev/null 2>&1; then
    printf 'OpenOCD executable not found: %s\n' "${openocd_exe}" >&2
    exit 1
fi

if [[ ! -f "${elf_path}" ]]; then
    printf 'ELF not found at %s\n' "${elf_path}" >&2
    exit 1
fi

"${openocd_exe}" \
    -f interface/cmsis-dap.cfg \
    -f target/rp2040.cfg \
    -c "adapter speed ${adapter_speed_khz}" \
    -c "program ${elf_path} verify reset exit"

printf 'Programmed %s over Debug Probe\n' "${elf_path}"