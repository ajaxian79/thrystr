#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source_dir="${1:-}"
dest="${repo_root}/src/gui/vendor/imgui"

if [[ -z "${source_dir}" || ! -f "${source_dir}/imgui.h" ]]; then
    echo "usage: $0 /path/to/imgui" >&2
    exit 2
fi

rm -rf "${dest}"
mkdir -p "${dest}/backends"

cp \
    "${source_dir}/LICENSE.txt" \
    "${source_dir}/imconfig.h" \
    "${source_dir}/imgui.cpp" \
    "${source_dir}/imgui.h" \
    "${source_dir}/imgui_draw.cpp" \
    "${source_dir}/imgui_internal.h" \
    "${source_dir}/imgui_tables.cpp" \
    "${source_dir}/imgui_widgets.cpp" \
    "${source_dir}/imstb_rectpack.h" \
    "${source_dir}/imstb_textedit.h" \
    "${source_dir}/imstb_truetype.h" \
    "${dest}/"

cp \
    "${source_dir}/backends/imgui_impl_glfw.cpp" \
    "${source_dir}/backends/imgui_impl_glfw.h" \
    "${source_dir}/backends/imgui_impl_opengl3.cpp" \
    "${source_dir}/backends/imgui_impl_opengl3.h" \
    "${source_dir}/backends/imgui_impl_opengl3_loader.h" \
    "${dest}/backends/"

{
    git -C "${source_dir}" describe --tags --always 2>/dev/null || echo "unknown"
    git -C "${source_dir}" rev-parse HEAD 2>/dev/null || echo "unknown"
} > "${dest}/IMGUI_VERSION.txt"
