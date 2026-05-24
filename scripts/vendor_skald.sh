#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source_dir="${1:-}"
dest="${repo_root}/src/gui/vendor/skald"
font_dest="${repo_root}/assets/fonts"

if [[ -z "${source_dir}" || ! -d "${source_dir}/include/skald" || ! -d "${source_dir}/src" ]]; then
    echo "usage: $0 /path/to/skald" >&2
    exit 2
fi

rm -rf "${dest}" "${font_dest}"
mkdir -p "${dest}/include" "${font_dest}"
cp -R "${source_dir}/include/skald" "${dest}/include/"
cp -R "${source_dir}/src" "${dest}/"
cp "${source_dir}/LICENSE" "${dest}/LICENSE.skald.txt"
cp -R "${source_dir}/fonts/." "${font_dest}/"

{
    git -C "${source_dir}" describe --tags --always 2>/dev/null || echo "unknown"
    git -C "${source_dir}" rev-parse HEAD 2>/dev/null || echo "unknown"
} > "${dest}/VERSION"
