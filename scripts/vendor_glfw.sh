#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
dest="${repo_root}/src/gui/vendor/glfw"
source_dir="${1:-}"
tmpdir=""

cleanup() {
    if [[ -n "${tmpdir}" && -d "${tmpdir}" ]]; then
        rm -rf "${tmpdir}"
    fi
}
trap cleanup EXIT

if [[ -z "${source_dir}" ]]; then
    tmpdir="$(mktemp -d)"
    source_dir="${tmpdir}/glfw"
    git clone --depth=1 --branch 3.4 https://github.com/glfw/glfw.git "${source_dir}"
fi

if [[ ! -f "${source_dir}/CMakeLists.txt" || ! -d "${source_dir}/include/GLFW" ]]; then
    echo "usage: $0 [/path/to/glfw]" >&2
    exit 2
fi

rm -rf "${dest}"
mkdir -p "${dest}"
cp -R \
    "${source_dir}/CMake" \
    "${source_dir}/CMakeLists.txt" \
    "${source_dir}/LICENSE.md" \
    "${source_dir}/deps" \
    "${source_dir}/include" \
    "${source_dir}/src" \
    "${dest}/"

{
    git -C "${source_dir}" describe --tags --always 2>/dev/null || echo "unknown"
    git -C "${source_dir}" rev-parse HEAD 2>/dev/null || echo "unknown"
} > "${dest}/VERSION"
