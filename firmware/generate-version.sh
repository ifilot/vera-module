#!/usr/bin/env bash
set -euo pipefail

firmware_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
version_file="${firmware_dir}/../VERSION"
output_file="${firmware_dir}/build/version.vh"

version="$(tr -d '\r\n' < "${version_file}")"
if [[ ! "${version}" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo "ERROR: VERSION must contain MAJOR.MINOR.PATCH, got '${version}'" >&2
    exit 1
fi

identifier="VERA v${version}"
if (( ${#identifier} > 15 )); then
    echo "ERROR: FPGA identifier '${identifier}' exceeds 15 ASCII characters" >&2
    exit 1
fi

mkdir -p "${firmware_dir}/build"
{
    echo '// Generated from the repository VERSION file. Do not edit.'
    for ((index = 0; index < 16; index++)); do
        if (( index < ${#identifier} )); then
            character="${identifier:index:1}"
            printf -v value '%d' "'${character}"
        else
            value=0
        fi
        printf '`define VERA_ID_BYTE_%d 8\x27h%02X\n' "${index}" "${value}"
    done
} > "${output_file}"

printf '%s\n' "${version}" > "${firmware_dir}/build/VERSION"
