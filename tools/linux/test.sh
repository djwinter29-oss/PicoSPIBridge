#!/usr/bin/env bash
set -euo pipefail

test_build_dir="${1:-build/tests}"

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../.." && pwd)"

cd "${repo_root}"

if [[ ! -d "${test_build_dir}" ]]; then
    cmake -S firmware/tests -B "${test_build_dir}"
    cmake --build "${test_build_dir}"
fi

ctest --test-dir "${test_build_dir}" --output-on-failure