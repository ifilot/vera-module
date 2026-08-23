#!/usr/bin/env bash
set -euo pipefail

firmware_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${firmware_dir}/.." && pwd)"
image_name="${VERA_TOOLCHAIN_IMAGE:-vera-oss-cad:2026-08-23}"

docker build \
    --tag "${image_name}" \
    "${repo_root}/toolchain/docker"

docker run \
    --rm \
    --user "$(id -u):$(id -g)" \
    --env HOME=/tmp \
    --volume "${repo_root}:/workspace" \
    --workdir /workspace \
    "${image_name}" \
    bash firmware/build.sh
