#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source_dir="${1:-}"
dest="${repo_root}/src/gui/vendor/stb"

if [[ -z "${source_dir}" || ! -f "${source_dir}/stb_image.h" || ! -f "${source_dir}/stb_image_write.h" ]]; then
    echo "usage: $0 /path/to/stb-wrapper-directory" >&2
    exit 2
fi

rm -rf "${dest}"
mkdir -p "${dest}"
cp \
    "${source_dir}/stb_image.h" \
    "${source_dir}/stb_image_impl.cpp" \
    "${source_dir}/stb_image_write.h" \
    "${source_dir}/stb_image_write_impl.cpp" \
    "${dest}/"
