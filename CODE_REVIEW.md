# thrystr — CODE_REVIEW.md

> Comprehensive expert review of the thrystr source tree, plus the plan to
> remove all external dependencies (skald, Dear ImGui, GLFW), introduce a
> dual-license LICENSE, raise clean-code quality to the project standard, and
> ship a documentation system (source-level + user-level + in-app search).
>
> Review baseline: branch `main` at commit `91775d1` (Merge #14, default
> workspace geometry). Files reviewed:
>
> - `CMakeLists.txt` (118 lines)
> - `include/thrystr/scalar_analysis.hpp` (106 lines)
> - `src/scalar_analysis.cpp` (591 lines)
> - `src/main.cpp` (3138 lines)
> - `tests/scalar_analysis_tests.cpp` (190 lines)
> - `third_party/stb/*`
> - `README.md`, `LICENSE` (current MIT)

---

## 0. TL;DR

| Area | Verdict | Notes |
|---|---|---|
| Algorithmic core (`scalar_analysis.*`) | **Solid**, narrow API, good tests on covered surface | A handful of edge-case gaps and one subtle ring-buffer comparison invariant worth tightening |
| Application/UI layer (`main.cpp`) | **Needs major restructuring** | 3138-line monolith, god `AppState`, ImGui calls intermixed with business logic, zero docstrings |
| Dependency posture | **Fragile** | `skald` pinned to floating `main` branch, ImGui discovered by directory scan, GLFW from system *or* git, none vendored |
| Licensing | **Needs replacement** | Current MIT does not capture the dual-use commercial-license intent |
| Documentation | **Absent** | No source docstrings, no user manual, no in-app help/search |
| Test coverage | **Partial** | Algorithmic core ~60 %; UI/serialization/file-dialog/main loop 0 % |

Recommended sequencing (no effort estimates — risk/complexity instead):

1. **Vendor + isolate dependencies** (medium complexity, low risk) — see §5.
2. **Replace LICENSE** (low complexity, low risk, dependency-blocking) — see §6.
3. **Split `main.cpp` into a `gui/` layer** (high complexity, medium risk) — see §4 + §5.
4. **Backfill tests around the serialization + selection logic** (medium complexity, medium risk) — see §3.4.
5. **Wire Doxygen + mdBook + in-app docs viewer into the build** (medium complexity, low risk) — see §7.

---

## 1. Bug findings (expert review)

Severity legend:
- **P1** — observable correctness bug or data-loss vector.
- **P2** — latent bug, brittle invariant, or wrong-on-platform-X.
- **P3** — code smell that will become a bug as the surrounding code changes.

### 1.1 Algorithmic core (`src/scalar_analysis.cpp`)

#### P1 — `scan_highest_entropy_window_stream` decrements `current_max_delta` past stale values

`src/scalar_analysis.cpp:131-141`

```cpp
--delta_counts[expired_delta];
while (current_max_delta > 0u &&
       delta_counts[current_max_delta] == 0u) {
    --current_max_delta;
}
```

`current_max_delta` is updated lazily by walking down from its last value
until it finds a slot with a positive count. That is fine for the streaming
running-max idiom **only because** the `delta_counts` histogram is dense.
But the **mapped** path can produce a delta of 0 whose count was never
incremented from a previous iteration (it can be reset to 0 between best
windows even if delta != 0 has higher counts). Then the while-loop will
walk all the way down to 0 and miss higher deltas that are still resident
elsewhere in the ring. **In practice the histogram covers the full ring so
this is safe today**, but the invariant is implicit. Recommendation:
either (a) replace with a monotonic-deque max (already used in
`find_highest_entropy_window`, see line 440), or (b) add an assertion that
the post-loop value equals `*std::max_element(delta_counts)` in debug builds.

#### P2 — `find_highest_entropy_window` may miss the leading window

`src/scalar_analysis.cpp:442-468`

The monotonic deque scan starts at `right=0` and only emits a window once
`right + 1 >= delta_span`. That part is correct. But the test on
`max_delta > best.max_delta` uses **strict greater** — when every window
has the same max delta (e.g. all-constant inputs with single transitions),
`best` stays at `{offset=0, max_delta=0, delta_index=0}` even when
the true earliest non-zero max sits in window 0. Reproducer:
`bytes = {5, 5, 5, 5}` with `window_bytes=2` → returns
`{offset=0, length=2, max_delta=0, delta_index=0}` (correct), but
`bytes = {5, 7, 5, 5}` → still returns offset 0 (correct). The actual
risk: combine this with the streamed mapped path; **the streamed path
records the first window where the max strictly increases, the in-memory
path records the first window where the max strictly increases too**, so
they agree. Promote to a test (§3.4) rather than a code change.

#### P2 — `copy_window_from_ring` relies on `length == ring.size()`

`src/scalar_analysis.cpp:53-61`

```cpp
output[i] = ring[(offset + i) % length];
```

Works only because `length` equals `window_length` equals `ring.size()`.
There is no assertion. If someone changes the streamed scanner to use a
larger or smaller ring than the window, the modulo math silently produces
garbage. **Fix:** take `ring_size` as a separate argument or add a
debug-only assertion that `length == ring.size()`.

#### P2 — `WindowEntropy.delta_index` semantics inconsistent across paths

In `scan_highest_entropy_window_stream` (`scalar_analysis.cpp:147-156`)
`delta_index` is a **global** index in the file (the position in the
delta sequence). In `find_highest_entropy_window` (line 466) it is also a
global index. In `analyze_file` (line 584) it is then subtracted from
`window.offset` to compute `max_delta_sample_index` which lives inside
the **returned window**. This is correct today but undocumented in the
public header and easy to break. Add a comment to `WindowEntropy` that
`delta_index` is in source-file coordinates.

#### P2 — `fit_wave_phase` floating-point: tolerance compared per-sample with `<=`

`src/scalar_analysis.cpp:519`

`std::abs(wave - static_cast<double>(scalars[i])) <= tolerance`

Using `<=` is fine, but when `tolerance == 0`, a strict comparison with
floats almost never hits; the call site default of
`kDefaultWaveTolerance = 1.0/128.0` avoids this, but a user can set it to
0 via the UI's `value_bar_double("wave tolerance", ...)`. Recommendation:
clamp `tolerance` to `std::max(tolerance, std::numeric_limits<double>::epsilon())`
in the public API, or reject `tolerance < 0`.

#### P3 — `unsigned_mod_256` truncates *then* fmods

`src/scalar_analysis.cpp:357-368`

`std::trunc(value)` followed by `std::fmod(truncated, 256.0)` is two
floating ops where one would do (`fmod(value, 256.0)`). Same end result
for finite values, but `trunc` of a very large finite value can lose
precision before the modulo. Replace with `std::fmod(value, 256.0)` and a
single `if (wrapped < 0.0) wrapped += 256.0;` correction.

#### P3 — Streamed scanner re-throws on file-size mismatch but EOF check is brittle

`src/scalar_analysis.cpp:184-189`

```cpp
if (!stop_scanning && !input.eof()) {
    throw std::runtime_error("could not read file: " + path.string());
}
if (!stop_scanning && index != result.source_size) {
    throw std::runtime_error("file size changed while reading: " + path.string());
}
```

If the file shrank during the scan we throw `"file size changed"`. If it
grew, we still hit `index != result.source_size` (we read more bytes than
expected) but the message is misleading. Split into two messages, or
detect direction.

#### P3 — `apply_value_mappers` per-stage finite check throws but does not localize

`src/scalar_analysis.cpp:347-355`

A non-finite intermediate throws with a generic message. For UX it would
help to include which mapper index produced the non-finite value. This is
a UI-quality concern; promote to follow-up.

### 1.2 Application/UI layer (`src/main.cpp`)

#### P1 — `load_wave_settings` v1 path leaks entity state from prior session

`src/main.cpp:1355-1402`

`if (version >= 2)` reads the entity table. For v1 files, `state.entities`
is **not cleared**. After loading a v1 file, the workspace will mix
previously-open entities with the newly-loaded source. Fix: clear
entities for v1 too, then `ensure_data_entity(state)` will rebuild from
the loaded source path.

#### P1 — `save_wave_settings` / `load_wave_settings` write host-endian raw bytes

`src/main.cpp:792-808, 1239-1416`

`write_binary<T>` writes the raw representation of `T` (including
`double`, `int`, `float`, enum widths) directly. The file is therefore
**not portable across endianness**. The magic + version handshake will
not catch endian-flipped files; they will load with garbage values.
Either (a) define an explicit serialization format (recommended:
fixed-width little-endian, with byte-swap on big-endian hosts), or (b)
embed a `kEndianStamp = 0x01020304u` in the header and refuse mismatched
files.

#### P1 — `Args::frames` parsing reads `int` from `atoi`

`src/main.cpp:218-224`

`std::atoi` returns 0 on parse failure with no way to distinguish from
`"0"`. The line `args.frames = std::max(0, std::atoi(argv[++i]))` will
treat `"foo"`, `""`, and `"0"` identically. Replace with `std::from_chars`
or `std::strtol` with error handling.

#### P2 — Out-of-bounds risk when `state.analysis->scalars.empty()` reaches `draw_plot`

`src/main.cpp:2462-2464`

```cpp
state.segment.selection_start = std::min(state.segment.selection_start, count - 1u);
state.segment.selection_end = std::min(state.segment.selection_end, count - 1u);
```

`count - 1u` underflows when `count == 0`. The function has an early
return at `state.analysis->scalars.empty()` (line 2446) so this is safe
today, but the underflow is one refactor away. Use `count > 0 ?
count - 1u : 0u` or move the clamp inside `clamp_segment` (which already
handles empty).

#### P2 — `force_undecorated_window` rewrites Motif hints to fixed-size struct

`src/main.cpp:265-288`

The struct is declared with `long input_mode` and `unsigned long`
elsewhere — on 32-bit/LP32 platforms this isn't 5 `long`s. The literal
`5` passed to `XChangeProperty` assumes a 5-element layout. On 64-bit
Linux it works. Add a `static_assert(sizeof(MotifWmHints) == 5 *
sizeof(long))` or use `XCB_MOTIF_WM_HINTS_*` constants from a real
header.

#### P2 — File-dialog state never clears `row_sel` when filter changes

`src/main.cpp:731-790`

`refresh_file_dialog_entries` rebuilds `state.file_dialog_rows`. If the
prior `row_sel` index is now out-of-range or now points at a different
file, the dialog still treats it as selected. Reset `row_sel = -1` when
the row count or filter changes.

#### P2 — `update_chrome_action` on X11 path ignores resize when only top/left is dragged

`src/main.cpp:597-609`

On X11 we call `XResizeWindow` with `w,h` only — we never move the
top-left origin. For ResizeTop / ResizeLeft / ResizeTopLeft the visible
window appears to "stretch from the wrong corner" because the X server
keeps the same origin. The local non-X11 path doesn't move it either
(`(void)x; (void)y;` at lines 613-614). This is a real UX bug visible
the moment a user drags the top-left handle. Combine `XMoveResizeWindow`
or call `glfwSetWindowPos` before `glfwSetWindowSize`.

#### P2 — `handle_custom_chrome` race: ImGui drag may begin same frame as chrome drag

`src/main.cpp:617-667`

A click that lands on the titlebar but inside an ImGui item registers as
a chrome `Move` action *and* gets consumed by ImGui. The
`!ImGui::IsAnyItemHovered()` guard at line 658 covers hovered items but
not popups (`IsAnyItemActive()` should be in the chain too). Symptom: a
menu opened by Ghost("File") and immediately drag-dismissed will start a
window-move.

#### P3 — `parse_args` silently truncates negative ints to 0

`src/main.cpp:220` — `std::max(0, std::atoi(...))`. A user passing
`--width -1` gets `width=0` with no message. Validate and warn.

#### P3 — `save_screenshot` allocates twice the framebuffer size

`src/main.cpp:838-852` — `pixels` plus `flipped` is `2 * w * h * 4`
bytes. For 4K renders that's ~64 MiB. Flip in place by swapping rows.

#### P3 — Magic numbers throughout layout code

Pixel constants (12.0f, 7.0f, 16.0f, 188.0f, 72.0f, ...) are scattered
across `draw_titlebar`, `draw_splash`, `draw_entity_toolbox`, `draw_plot`,
`draw_settings_dialog`. Promote to named constants near
`kTopChromeHeight` or, better, a `Layout` namespace.

#### P3 — `value_bar_double` tooltip says the same thing whether pressed or hovered

`src/main.cpp:1770` — `skald::Tooltip(pressed ? "Drag right or left" : "Drag right or left");`
Same string twice. Either delete the ternary or differentiate.

#### P3 — `nice_tick` returns `std::size_t` but takes a double — narrowing risk for raw < 0

`src/main.cpp:854-869` — Inputs are computed via division by `x_step`
which can in principle be negative due to a sign flip. Document the
precondition or clamp.

### 1.3 Build system (`CMakeLists.txt`)

#### P1 — `skald` pinned to `GIT_TAG main`

`CMakeLists.txt:32` — pinning a dependency to a moving branch makes
builds non-reproducible. Pin to a tag or commit SHA. (See §5.)

#### P1 — Backend discovery by directory scan

`CMakeLists.txt:64-83`

```cmake
get_target_property(THRYSTR_IMGUI_INCLUDE_DIRS imgui INTERFACE_INCLUDE_DIRECTORIES)
foreach(dir IN LISTS THRYSTR_IMGUI_INCLUDE_DIRS)
    if(EXISTS "${dir}/backends/imgui_impl_glfw.cpp")
        set(imgui_SOURCE_DIR "${dir}")
        ...
```

Walks include dirs looking for a backend source file. Brittle: depends on
how `skald` chose to install ImGui. Vendoring (see §5) removes this.

#### P2 — `glfw3` found-version not enforced

`CMakeLists.txt:51-62` — Either system glfw3 or git-fetched `3.4`.
System packages may be older; ImGui backend differences across GLFW
versions are subtle. Pin to a specific version regardless of source.

#### P3 — `THRYSTR_BUILD_APP=OFF` does not skip stb either

`CMakeLists.txt:86-89` — stb is only used by the app; the static lib for
`thrystr_stb_image_write` is built inside the `if(THRYSTR_BUILD_APP)`
block. OK as-is.

---

## 2. Clean-code audit

The project's standing rules (`WORKSPACE_RULES.md` §6) demand single
responsibility per file/class, clear OO abstractions, both unit and
integration tests, and (per the user's directive in this review) **docstrings
on every function** and **functions kept small with well-named helpers**.

### 2.1 File-size violations

| File | Lines | Verdict |
|---|---:|---|
| `src/main.cpp` | 3138 | **Egregious SRP violation.** UI rendering, event handling, custom WM-chrome, file I/O, serialization, fit math, file dialog, splash, settings dialog, and the GLFW main loop all live in one TU. |
| `src/scalar_analysis.cpp` | 591 | Acceptable. One responsibility. |
| `tests/scalar_analysis_tests.cpp` | 190 | Acceptable. |

### 2.2 Functions that exceed reasonable size / responsibility

| Function | Lines | Issue |
|---|---:|---|
| `draw_plot` | 266 (2433–2698) | Renders axes, grid, max-delta marker, selection band, mouse-pick + drag handles, data line, all waves, data points, plot border, legend, scrolling, ctrl-scroll-zoom. Should be five-plus functions. |
| `draw_entity_toolbox` | 168 (2182–2350) | Section list, data inspector, x-line section editor, name editor, per-type load-params editor. Split per panel. |
| `save_wave_settings` | 72 (1239–1310) | Mixes path normalization, format negotiation, raw write, status messaging. |
| `load_wave_settings` | 104 (1312–1416) | Same as above plus version branching. Two functions: `read_header_v1` / `read_entities_v2`. |
| `draw_splash` | 148 (1981–2128) | Lays out splash chrome + recents + actions. The action_row lambda belongs as a named helper. |
| `begin_chromed_file_dialog` | 107 (2741–2848) | Pushes style + builds modal + table + filename row + buttons. Three functions. |
| `draw_titlebar` | 132 (1601–1732) | Renders bar background, brand, three menus (File/Create/Settings), source label, status, three window controls. Split per concern. |
| `fit_wave_to_selection` | 39 (1098–1137) | Acceptable. |
| `scan_highest_entropy_window_stream` | 135 (63–197) | Long but cohesive. Helper-extractable. |

### 2.3 Naming

Most names are clear and idiomatic. Specific call-outs:

- `kKinds[]` (line 1803) — non-descriptive; rename `kValueMapperShortNames`.
- `value_bar_double` / `value_bar_int` — `bar` implies a horizontal indicator; these are *drag* fields. Rename `drag_field_double` or `scrub_field_double`.
- `should_defer_phase_fit_on_load` — fine but the threshold (`128 MiB`) is hard-coded; move to a named constant exposed at file scope.
- `next_string` / `next_int` lambdas in `parse_args` — fine inline but two lambdas inside a switch-like loop is heavier than a tiny `class ArgParser`.
- `AppState` (line 163) — god struct. See §4.

### 2.4 Docstrings

**Zero functions have docstrings.** The user's rule for this project is
that *all functions must have doc strings*. The CMake plan in §7 wires
Doxygen as a build step; the source must be annotated to feed it.

Convention to adopt (Doxygen, brief + detailed, params, returns,
throws):

```cpp
/// Scan a byte buffer for the contiguous window with the largest
/// adjacent-byte delta.
///
/// Uses an O(n) monotonic-deque sliding-max; allocates only the deque.
///
/// @param bytes         The bytes to scan; may be empty.
/// @param window_bytes  Window length in bytes. Clamped to bytes.size().
/// @return The earliest window whose max adjacent delta is largest.
WindowEntropy find_highest_entropy_window(std::span<const std::uint8_t> bytes,
                                          std::size_t window_bytes);
```

For UI helpers, document side effects (modal opens, draw-list mutation,
state mutations) explicitly. Adopt `///` triple-slash style — Doxygen,
clangd, and IDEs all parse it.

### 2.5 Function-size and `inline`

C++ does not penalize small functions when the compiler can inline them.
Reorganize the long functions in §2.2 into ~20-line cohesive helpers
declared `inline` (or `[[gnu::always_inline]]` where genuinely hot) so
performance is preserved. The current monolithic functions optimize
nothing the inliner can't do for you, and they actively defeat the human
reader. Specifically:

- Promote `action_row` lambda inside `draw_splash` to a named
  `draw_splash_action_row` (file-scope, `inline`).
- Promote per-row mapper rendering inside `draw_value_mapper_stack` to
  `draw_mapper_row(state, index)`.
- Promote selection drag handling in `draw_plot` to
  `update_selection_from_mouse(state, plot_left, x_step, count)`.
- Promote axis/grid rendering to `draw_plot_axes(...)`.
- Promote wave overlay to `draw_wave_overlay(...)`.

### 2.6 `AppState` is a god struct (line 163-207)

Currently mixes:

- App config (`max_slope`, `wave_scale`, `phase_steps`, ...).
- Persistent UI buffers (`path[4096]`, `wave_path[4096]`, `entity_name[128]`).
- Analysis output (`std::optional<thrystr::Analysis>`).
- Workspace data (`entities`, `segment`, `mappers`).
- Transient UI state (`chrome_action` + 8 chrome-drag fields, `selection_drag_handle`, `request_close`, `splash_open`).
- File-dialog state (`pending_dialog`, `active_dialog`, `file_dialog`, `file_dialog_rows`, `file_dialog_entries`, `file_dialog_last_row`).
- Font handles (`fonts`).

Split into composable members:

```cpp
struct AppPaths        { /* path, wave_path */ };
struct AnalysisParams  { /* max_slope, wave_scale, wave_tolerance, phase_steps, phase_test_points */ };
struct Workspace       { /* analysis, entities, segment, mappers, status */ };
struct WindowChromeState { /* chrome_action, chrome_start_* */ };
struct FileDialog      { /* pending, active, state, rows, entries, last_row */ };
struct App             { /* aggregates the above + fonts + flags */ };
```

This is the lever that makes splitting `main.cpp` into a `gui/` layer
tractable.

### 2.7 Other clean-code issues

- Raw `char[4096]` buffers for paths — use `std::string` and copy into a
  small `std::array<char, N>` *only* at the ImGui InputText boundary.
- `static_cast<int>(state.mappers.size())` for ImGui::PushID — fine but
  repeats; helper `imgui::PushID(std::size_t)` overload would centralize.
- `format_bytes` / `format_count` build a `std::string` per frame —
  fine, but mark hot allocation sites.
- The mapper stack `static constexpr const char* kKinds[]` is local to
  `draw_value_mapper_stack` — promote to `thrystr::ValueMapperKindName(kind)`
  in the core library so callers can use it for tooltips, exports, etc.

---

## 3. Test coverage

### 3.1 What is covered today

`tests/scalar_analysis_tests.cpp` tests (190 lines, 10 cases):

1. `byte_to_scalar` mapping at 0 / 128 / 255.
2. `map_byte_to_wrapped` for add/sub/mul/div/disabled.
3. Divide-by-zero rejection.
4. `adjacent_delta` corner values.
5. `find_highest_entropy_window` on an 8-byte fixture.
6. `compute_x_scale` on slope and flat inputs.
7. `fit_wave_phase` smoke test (sine).
8. `fit_wave_phase` honors `wave_scale=2`.
9. `analyze_file` end-to-end with a mapper stack.
10. `analyze_file` streamed best-window selection on an 8-byte fixture.

### 3.2 Coverage gaps in the algorithmic core

These tests *must* exist before §5's vendoring work, because the
vendoring rebuilds the build graph and risks regressions:

| Gap | Why it matters |
|---|---|
| `unsigned_mod_256` on large positive (`> 2^53`), large negative, NaN, +∞, −∞ | Public API; throws on non-finite; behavior at the float-precision boundary undefined |
| `apply_value_mappers` non-finite propagation between stages | Reported error currently does not name the failing stage |
| `find_highest_entropy_window` on `bytes.size() < 2`, `bytes.size() == window_bytes`, all-zero, all-equal, `window_bytes == 1` | Window-of-one shortcut path |
| `find_highest_entropy_window` parity with `scan_highest_entropy_window_stream` | Two implementations must agree |
| `scan_highest_entropy_window_mapped` vs streaming parity on real files | mmap path returns same `WindowEntropy` and same `bytes` |
| Streaming path with a mapper stack at the 256-byte transition (wrap edge) | Mapped histogram correctness |
| `fit_wave_phase` with `tolerance == 0` | Should not infinite-loop or assert |
| `read_file_bytes` on empty file, directory path, unreadable file | Exception path coverage |
| `analyze_file` with `phase_test_points == 0` (deferred-fit case) | Confirms PhaseFit returns zero-hit defaults |
| `Analysis.max_delta_sample_index` correctness when `delta_index < window.offset` | Currently the `if` guards but no test exercises both sides |

### 3.3 Coverage gaps in the application layer

The entire `main.cpp` has **no tests**. Achievable targets:

- **Pure helpers** (no GLFW/ImGui needed): `parse_args`, `format_bytes`,
  `format_count`, `nice_tick`, `data_spatial_period_nm`, `clamp_index`,
  `normalized_selection`, `with_wave_settings_extension`,
  `entry_matches_filter`, `home_or_current_path`, `should_defer_phase_fit_on_load`,
  `wave_wavelength_at_nm`, `wave_value_at_nm`, `score_wave_on_range`,
  `wave_fit_is_better`, `log_lerp`, `fit_wave_to_selection`,
  `rebuild_wavelength_modifiers`. After §4's split these should live in
  `app/` (non-UI) and be unit-tested directly.

- **Serialization round-trip**: write a `.thryw` file, read it back, assert
  full structural equality of `state`. Add for both v1 (older file
  without entities) and v2 formats.

- **Endianness**: build with `-DTHRYSTR_FORCE_BIG_ENDIAN_TEST=ON` on
  hosts that support `htobe64`, or hand-author a fixture file with
  endian-swapped fields and assert we reject (or correctly handle) it.

- **Headless integration test** for the render path: run
  `thrystr --file fixture.bin --screenshot /tmp/out.png --frames 2` in
  CI on Linux with `xvfb-run`, assert the PNG is non-empty and has the
  expected dimensions. (Headless smoke only — no visual regression yet.)

### 3.4 Recommended new test files

Once `main.cpp` is split (see §4), create:

- `tests/app_serialization_tests.cpp` — wave-settings round-trip,
  cross-version load, source-path absent/present.
- `tests/app_selection_tests.cpp` — `clamp_segment`,
  `normalized_selection`, `analysis_selection_bounds`, selection drag
  handle math.
- `tests/app_wave_fit_tests.cpp` — `fit_wave_to_selection`,
  `rebuild_wavelength_modifiers`, `score_wave_on_range`.
- `tests/gui_layout_tests.cpp` — pure layout math helpers (`plot_x_step`,
  `y_for_value`, `nice_tick`).
- `tests/integration/render_smoke_test.sh` — runs the headless
  `--screenshot` path, asserts PNG size + decode.

Add CTest entries; gate the integration test behind a CMake option so it
isn't required on machines without GL.

---

## 4. Encapsulated GUI layer

The directive is: bring **all** of skald, **all** of ImGui, and **all** of
GLFW into the application code in a *clean encapsulated GUI layer*.
"Encapsulated" here means the rest of the codebase should not need to
know that ImGui / GLFW / skald exist.

### 4.1 Proposed layout after the split

```
include/thrystr/                  -- public API of the algorithmic core
  scalar_analysis.hpp
  app/
    args.hpp                      -- Args + parse_args
    workspace.hpp                 -- Workspace, Entity, Segment, Mappers
    wave_settings_io.hpp          -- save/load .thryw
    wave_fit.hpp                  -- score/fit/rebuild helpers
    layout.hpp                    -- y_for_value, nice_tick, plot_x_step

src/                              -- algorithmic + app (no UI)
  scalar_analysis.cpp
  app/
    args.cpp
    workspace.cpp
    wave_settings_io.cpp
    wave_fit.cpp
    layout.cpp

src/gui/                          -- the encapsulated GUI layer
  gui.hpp                         -- the ONLY header used by main()
  gui.cpp                         -- thin orchestration; owns App
  window/
    window_manager.hpp/.cpp       -- wraps GLFW init, window create, swap
    chrome.hpp/.cpp               -- custom WM chrome + drag/resize
    cursors.hpp/.cpp              -- chrome cursor set
  render/
    gl_context.hpp/.cpp           -- glClear/glViewport/glReadPixels
    screenshot.hpp/.cpp           -- PNG writer (wraps stb)
  ui/
    titlebar.hpp/.cpp
    splash.hpp/.cpp
    entity_toolbox.hpp/.cpp
    settings_dialog.hpp/.cpp
    inspector_overlay.hpp/.cpp
    file_dialog.hpp/.cpp
    plot_view.hpp/.cpp
    mapper_stack.hpp/.cpp
    widgets/                      -- value_bar_double, window_control_button, etc.
  docs_panel/                     -- see §7
    docs_panel.hpp/.cpp           -- in-app help (search + screen-scoped)

src/gui/vendor/                   -- the vendored third-party code
  imgui/                          -- full Dear ImGui source (see §5.2)
  glfw/                           -- full GLFW source (see §5.3)
  skald/                          -- full skald source (see §5.1)
  stb/                            -- stb_image_write.h (already present)

src/main.cpp                      -- ~50 lines: parse args, construct GUI, run
```

Key rule: **no header outside `src/gui/` may `#include` ImGui, GLFW, or
skald**. The algorithmic and app layers stay UI-agnostic and testable
without an OpenGL context.

### 4.2 Layer boundaries

```
+------------+        +---------+
|   main     |  --->  |   Gui   |   (the only thing main() sees)
+------------+        +---------+
                          |
                          v
+---------------------------------------------+
|     gui::App  (composition of subviews)     |
+---------------------------------------------+
   |          |          |             |
   v          v          v             v
+------+ +--------+ +-----------+ +-----------+
|window| |titlebar| |plot_view  | |docs_panel |
+------+ +--------+ +-----------+ +-----------+
   |          |          |             |
   +----------+----------+-------------+
              |
              v
+---------------------------------------------+
|       vendored ImGui + GLFW + skald         |
+---------------------------------------------+

+---------------------------------------------+
|       thrystr::* (algorithmic core)         |
|  scalar_analysis, workspace, wave_fit,      |
|  wave_settings_io, args  (no UI deps)       |
+---------------------------------------------+
```

The `gui::` layer depends on the algorithmic layer; the algorithmic
layer never depends on `gui::`.

### 4.3 Public GUI surface

`include/thrystr/gui/gui.hpp` — the only public GUI header — would expose:

```cpp
namespace thrystr::gui {

/// Configuration for one application lifetime. Owns nothing GL-related;
/// safe to construct before GLFW init.
struct Config {
    Args   args;
    /// Absolute path to the bundled fonts (Inter, Noto, Skald icons).
    std::filesystem::path font_dir;
};

/// Run the application. Owns the GLFW + ImGui + skald lifecycle.
/// Returns the process exit code.
int Run(const Config& config);

} // namespace thrystr::gui
```

`main.cpp` shrinks to:

```cpp
int main(int argc, char** argv) {
    thrystr::gui::Config cfg;
    cfg.args = thrystr::ParseArgs(argc, argv);
    cfg.font_dir = THRYSTR_BUNDLED_FONT_DIR;
    return thrystr::gui::Run(cfg);
}
```

### 4.4 Step-wise migration plan

Each step is independently buildable and testable; no big-bang rewrite.

1. **Extract pure helpers into `thrystr::app`** (no UI dependency). Move
   `parse_args`, `format_bytes`, `nice_tick`, `wave_value_at_nm`,
   `wave_wavelength_at_nm`, `score_wave_on_range`, `wave_fit_is_better`,
   `log_lerp`, `fit_wave_to_selection`, `rebuild_wavelength_modifiers`,
   `clamp_index`, selection helpers, and `.thryw` serialization out of
   `main.cpp`. Add unit tests (§3.4).
2. **Split `AppState`** per §2.6 into composable structs in
   `thrystr::app`.
3. **Introduce `thrystr::gui::WindowManager`** that owns the GLFW
   lifecycle. Lift `prefer_x11_when_available`, `create_undecorated_window`,
   `force_undecorated_window`, `apply_window_geometry`,
   `default_workspace_geometry`, `glfw_error`.
4. **Introduce `thrystr::gui::Chrome`** that owns custom-chrome state,
   the cursors, the resize/move handlers, and the Motif-hints
   indirection. This finally has a home for the `XMoveResizeWindow` fix
   (P2 in §1.2).
5. **Introduce `thrystr::gui::ScreenshotWriter`** that wraps stb and the
   row-flip.
6. **Extract every `draw_*` function** into its own `ui/` translation
   unit. Each `draw_*` becomes a class with explicit
   `Render(const State&, ...)` interface; state mutations are returned
   as Events (`StartupActionRequested`, `OpenFileDialog{purpose}`,
   `CreateWave`, `Refresh`).
7. **Introduce `thrystr::gui::App`** which composes the sub-views and
   owns the per-frame loop. The whole `main()` GLFW init/shutdown lives
   here.
8. **Delete the old `main.cpp` body**, replacing with the 6-line
   bootstrapper in §4.3.
9. **Move ImGui include directives** to only the gui translation units.
10. **Run the full test suite** after each step; the algorithmic tests
    must keep passing throughout.

### 4.5 Why this is worth doing now (before vendoring)

Vendoring (§5) physically pulls thousands of source files into the tree.
Doing it on top of the current `main.cpp` would entangle vendored
third-party code with our application logic and make every future
upgrade painful. Splitting first means the vendored trees live under
`src/gui/vendor/` and only `src/gui/` includes them.

---

## 5. Dependency vendoring plan

### 5.1 skald (license-owner consent given)

**Goal:** remove all `FetchContent_Declare(skald)` and
`THRYSTR_SKALD_SOURCE_DIR` indirection. skald's source becomes part of
the thrystr tree.

**Surface used by thrystr** (grepped from `main.cpp`):

- Functions: `ApplyDefaults`, `ApplyAccent`, `LoadFonts`, `SectionHeader`,
  `MutedSeparator`, `AccentButton`, `GhostButton`, `IconButton`, `Tooltip`,
  `PillToggle`, `BadgeChip`, `KvRow`, `KvRowStatus`, `BeginPanel`,
  `EndPanel`.
- Types: `Fonts`, `FileDialogState`, `FileDialogEntry`, `FileDialogMode`,
  `FileDialogResult`, `BadgeTone`.
- Tokens: `surface::{deep,window,panel,panel_alt,control,control_hi,control_act,input}`,
  `ink::{primary,muted,dim}`, `border::{default_,separator,strong}`,
  `status::destructive`, `accents::{chrome,violet,gold,mint,coral,cyan}`,
  `radii::{ctrl,pill,card,modal}`, `to_vec4`, `with_alpha`.
- Icons: `icons::{kFile,kFolder,kSave,kPlus,kCog}`.
- Fonts data dir: `THRYSTR_SKALD_FONT_DIR` macro.

**Repo source** to copy (from `repos/skald/`):

| Path | Bytes (≈) | Notes |
|---|---:|---|
| `include/skald/*.h` (6 files, 487 LoC) | small | Public API surface |
| `src/{theme,widgets,fonts}.cpp` (1201 LoC) | small | Implementations |
| `fonts/*.ttf` (Inter Regular/Medium, NotoSansMono, SkaldIcons) | ~600 KiB | Bundled font assets |
| `icons/*.svg` source (for regeneration) | small | Reference only — `icons.gen.h` already committed |
| `LICENSE` | ~1 KiB | Preserved verbatim per attribution |

**Vendoring procedure:**

1. Copy `repos/skald/include/skald/` → `src/gui/vendor/skald/include/skald/`.
2. Copy `repos/skald/src/` → `src/gui/vendor/skald/src/`.
3. Copy `repos/skald/fonts/` → `assets/fonts/` (renamed from "skald"
   because we own it now; keep the original font OFL/Noto-LICENSE
   files alongside).
4. Copy `repos/skald/LICENSE` to `src/gui/vendor/skald/LICENSE.skald.txt`
   so attribution is preserved even though we own the code.
5. Replace the `find_package`/`FetchContent` block with:

   ```cmake
   add_library(thrystr_skald STATIC
       src/gui/vendor/skald/src/theme.cpp
       src/gui/vendor/skald/src/widgets.cpp
       src/gui/vendor/skald/src/fonts.cpp)
   target_include_directories(thrystr_skald PUBLIC
       ${CMAKE_CURRENT_SOURCE_DIR}/src/gui/vendor/skald/include)
   target_link_libraries(thrystr_skald PUBLIC thrystr_imgui)
   add_library(thrystr::skald ALIAS thrystr_skald)
   ```

6. Replace every `skald::Foo` reference with `thrystr::skin::Foo` after
   wrapping (next step) — or, simpler and explicit: keep the `skald::`
   namespace internally but mark the headers as private API by living
   under `src/gui/vendor/`. Only `src/gui/` includes them.

7. Delete `THRYSTR_SKALD_SOURCE_DIR` and `FetchContent_Declare(skald)`
   from `CMakeLists.txt`.

8. Update `THRYSTR_SKALD_FONT_DIR` to point at `assets/fonts` at
   configure time:

   ```cmake
   target_compile_definitions(thrystr PRIVATE
       THRYSTR_BUNDLED_FONT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts")
   ```

9. Run the full test suite, manually open the splash, confirm fonts
   load.

**Risk:** none — we own the skald license.

### 5.2 Dear ImGui

**Goal:** remove the `FetchContent` indirection (currently transitive
through skald). Add ImGui as a first-class vendored library.

**Source to vendor** (from `https://github.com/ocornut/imgui`):

| Subset | Files | Purpose |
|---|---|---|
| Core | `imgui.{h,cpp,_internal.h}`, `imgui_draw.cpp`, `imgui_tables.cpp`, `imgui_widgets.cpp`, `imconfig.h`, `imstb_*.h` | Library |
| Backends | `backends/imgui_impl_glfw.{h,cpp}`, `backends/imgui_impl_opengl3.{h,cpp}` | Required by thrystr |
| Optional | `imgui_demo.cpp` | Skip in release; keep behind `-DTHRYSTR_IMGUI_DEMO=ON` |
| License | `LICENSE.txt` | Preserve verbatim |

**Pin** to v1.91.5-docking (current skald pin) and record the SHA in
`src/gui/vendor/imgui/VERSION`. Renew on a deliberate cadence; do not
auto-update.

**CMake target:**

```cmake
add_library(thrystr_imgui STATIC
    src/gui/vendor/imgui/imgui.cpp
    src/gui/vendor/imgui/imgui_draw.cpp
    src/gui/vendor/imgui/imgui_tables.cpp
    src/gui/vendor/imgui/imgui_widgets.cpp
    src/gui/vendor/imgui/backends/imgui_impl_glfw.cpp
    src/gui/vendor/imgui/backends/imgui_impl_opengl3.cpp)
target_include_directories(thrystr_imgui SYSTEM PUBLIC
    src/gui/vendor/imgui
    src/gui/vendor/imgui/backends)
target_link_libraries(thrystr_imgui PUBLIC thrystr_glfw OpenGL::GL)
add_library(thrystr::imgui ALIAS thrystr_imgui)
```

`SYSTEM PUBLIC` suppresses warnings from the vendored headers under
`-Wall -Wextra -Wpedantic`. ImGui is MIT-licensed; preserve `LICENSE.txt`
under `src/gui/vendor/imgui/`.

**Configuration:** drop an `imconfig.h` patch (or use the upstream
`imconfig.h` unchanged) and document any defines in
`src/gui/vendor/imgui/IMGUI_THRYSTR.md` so future readers know which
toggles we expect.

### 5.3 GLFW

**Goal:** clone the GLFW repo into the tree (the user's wording).
"Clone" here means add a git submodule **or** a vendored snapshot — pick
one; both are acceptable but they have different upgrade ergonomics.

**Recommendation:** vendored snapshot (no submodule). Snapshots survive
shallow clones, archive downloads, CI caches, and corporate networks
that block submodule URLs. Submodules entangle thrystr's git history
with GLFW's.

**Source to vendor** (from `https://github.com/glfw/glfw` tag `3.4`):

| Path | Notes |
|---|---|
| `CMakeLists.txt` plus `src/`, `include/`, `deps/` | The library itself |
| `cmake/` | Platform detection |
| `LICENSE.md` | BSD-like (zlib/libpng-derived); preserve verbatim |
| Skip: `docs/`, `examples/`, `tests/`, `.github/` | Not needed for our build |

**Integration:**

```cmake
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
add_subdirectory(src/gui/vendor/glfw EXCLUDE_FROM_ALL)
add_library(thrystr::glfw ALIAS glfw)
```

Delete the `find_package(glfw3 QUIET)` branch entirely; we are no longer
supporting system GLFW. This eliminates a class of `"works on my box"`
bugs caused by older system packages.

**Snapshot procedure** (documented in `scripts/vendor_glfw.sh`):

```bash
#!/usr/bin/env bash
set -euo pipefail
TAG="3.4"
TMPDIR="$(mktemp -d)"
git clone --depth=1 --branch "$TAG" https://github.com/glfw/glfw.git "$TMPDIR/glfw"
rm -rf src/gui/vendor/glfw
mkdir -p src/gui/vendor/glfw
cp -r "$TMPDIR/glfw/CMakeLists.txt" \
      "$TMPDIR/glfw/cmake" \
      "$TMPDIR/glfw/src" \
      "$TMPDIR/glfw/include" \
      "$TMPDIR/glfw/deps" \
      "$TMPDIR/glfw/LICENSE.md" \
      src/gui/vendor/glfw/
echo "$TAG" > src/gui/vendor/glfw/VERSION
rm -rf "$TMPDIR"
```

A similar `scripts/vendor_imgui.sh` and `scripts/vendor_skald.sh` covers
the other two. Each script writes a `VERSION` file with the pinned tag.
Updates are a single command and are visible as a single commit.

### 5.4 stb (already vendored)

`third_party/stb/stb_image_write.{h,cpp}` is fine. Move it to
`src/gui/vendor/stb/` for layout consistency.

### 5.5 Resulting `CMakeLists.txt` skeleton

```
thrystr_core           -- algorithmic library (no UI)
thrystr_app            -- workspace/serialization/fit (no UI)
thrystr_glfw           -- vendored GLFW
thrystr_imgui          -- vendored ImGui + backends
thrystr_skald          -- vendored skald (links thrystr_imgui)
thrystr_stb            -- vendored stb_image_write
thrystr_gui            -- our UI layer (links the four vendored libs + core + app)
thrystr                -- 5-line main() (links thrystr_gui)
thrystr_*_tests        -- per layer
```

This finally makes the dependency graph readable in `cmake --graphviz`.

### 5.6 Build-reproducibility checklist

- All three vendor `VERSION` files committed.
- Network access not required during `cmake` or `cmake --build`.
- Removing `~/.cache/cmake-fetchcontent` does not break the build.
- `git submodule status` is empty.
- Bumping a vendored library is a deliberate, reviewable commit.

---

## 6. LICENSE

The current `LICENSE` is MIT (1-clause permissive). The directive is to
replace it with a **dual license**:

- **Non-commercial use**: free to modify and compile for personal or
  public use, provided redistribution includes the source.
- **Commercial use**: must contact `hello@blaketullysmith.com` for a
  paid license whose scope and price depend on the use case.

### 6.1 License text

Write the new `LICENSE` at the repo root, replacing the existing MIT
file:

```
thrystr — Dual License
Copyright (c) 2026 Blake Tully Smith

This software is offered under two licenses. Choose the one that
matches your use:

──────────────────────────────────────────────────────────────────────
1. NON-COMMERCIAL LICENSE  (the "Personal & Public License")
──────────────────────────────────────────────────────────────────────

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to use, copy, modify, and compile the Software for any
NON-COMMERCIAL purpose, including private personal use and publicly
distributed non-commercial use, subject to the following conditions:

  (a) Source-availability. Any binary you distribute must be
      accompanied by, or include a clearly visible link to, the
      complete corresponding source code under this same license.
      "Complete corresponding source" means the source you compiled
      from, including your modifications, in a form a competent
      developer can use to build the same binary.

  (b) Attribution. This copyright notice and license text must be
      retained verbatim in every copy or substantial portion of the
      Software, including modified versions.

  (c) Notices. The names of the author and contributors may not be
      used to endorse or promote derived works without specific
      prior written permission.

"Non-commercial" means use that is not primarily directed toward,
nor intended for, commercial advantage or monetary compensation.
Internal use by a for-profit entity in furtherance of its business
is NOT non-commercial, even if the resulting binary is not sold.

──────────────────────────────────────────────────────────────────────
2. COMMERCIAL LICENSE
──────────────────────────────────────────────────────────────────────

If your intended use is commercial — including, without limitation,
use within a for-profit organization's products, services, internal
tooling, paid consulting deliverables, SaaS, or any context where
the Software contributes to revenue or business operations — you
must obtain a separate Commercial License.

To request a commercial license, email:

    hello@blaketullysmith.com

with a description of:
  - the entity requesting the license,
  - the intended use case,
  - whether the use is internal or distributed,
  - the deployment scale (rough seat count, user count, or instance
    count is enough to start),
  - any redistribution or sub-licensing requirements.

License terms, scope, and fees are determined per request and
depend on the use case and intent. Until a Commercial License is
executed, commercial use is not permitted.

──────────────────────────────────────────────────────────────────────
3. WARRANTY DISCLAIMER  (applies to both licenses)
──────────────────────────────────────────────────────────────────────

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

──────────────────────────────────────────────────────────────────────
4. VENDORED THIRD-PARTY CODE
──────────────────────────────────────────────────────────────────────

This repository vendors copies of the following third-party libraries
under their respective licenses, which are preserved verbatim in
src/gui/vendor/<name>/LICENSE*:

  - Dear ImGui (MIT)   - src/gui/vendor/imgui/LICENSE.txt
  - GLFW (zlib/libpng) - src/gui/vendor/glfw/LICENSE.md
  - skald (MIT)        - src/gui/vendor/skald/LICENSE.skald.txt
  - stb (public domain / MIT) - src/gui/vendor/stb/stb_image_write.h header

These licenses govern the vendored code itself. Your use of the
thrystr application as a whole is governed by sections 1–3 above.
```

### 6.2 README header

Add a banner at the top of `README.md`:

```
> Dual-licensed: free for non-commercial use under the Personal &
> Public License (see LICENSE §1); commercial use requires a paid
> license (see LICENSE §2, contact hello@blaketullysmith.com).
```

### 6.3 SPDX identifier

Add `// SPDX-License-Identifier: LicenseRef-thrystr-dual` to the top of
every first-party source/header. The `LicenseRef-` prefix is the SPDX
convention for custom licenses; the rest of the identifier matches the
license filename without extension.

### 6.4 LICENSE compatibility notes

- **Vendored MIT/zlib code** (ImGui, GLFW, skald, stb) remains under
  its own license inside `src/gui/vendor/`. Our dual license applies to
  the *thrystr-original* source.
- The current MIT grant is **broader** than the new dual license. After
  the switch, downstream users cannot rely on the MIT permissions for
  any new commits — but historical MIT-licensed commits still exist in
  git history. Document this in `LICENSE` or a `LICENSE.md` note for
  legal clarity.

### 6.5 Recommended companion files

- `LICENSE-COMMERCIAL.md` — a one-page template you can paste into a
  reply when a commercial-license request arrives.
- `NOTICE` — short attribution file listing every vendored library and
  its upstream URL.

---

## 7. Documentation system

### 7.1 Source-code documentation (Doxygen)

**Target:** every public/private function and class carries a `///`
docstring (the rule the user just affirmed). On every build,
human-readable source docs are generated under `build/docs/source/`.

**Toolchain:** Doxygen + Doxygen-Awesome-CSS (clean dark theme, fits the
skald look). No graphviz required for v1.

**CMake wiring** (`docs/CMakeLists.txt`):

```cmake
option(THRYSTR_BUILD_DOCS "Generate source + user documentation" ON)

if(THRYSTR_BUILD_DOCS)
    find_package(Doxygen REQUIRED dot OPTIONAL)
    set(DOXYGEN_PROJECT_NAME           "thrystr")
    set(DOXYGEN_PROJECT_BRIEF          "Skald/OpenGL/ImGui scalar entropy waveform explorer")
    set(DOXYGEN_OUTPUT_DIRECTORY       "${CMAKE_BINARY_DIR}/docs/source")
    set(DOXYGEN_GENERATE_HTML          YES)
    set(DOXYGEN_GENERATE_LATEX         NO)
    set(DOXYGEN_EXTRACT_ALL            YES)
    set(DOXYGEN_EXTRACT_PRIVATE        YES)
    set(DOXYGEN_EXTRACT_STATIC         YES)
    set(DOXYGEN_RECURSIVE              YES)
    set(DOXYGEN_EXCLUDE_PATTERNS       "*/vendor/*" "*/build/*")
    set(DOXYGEN_WARN_AS_ERROR          NO)
    set(DOXYGEN_HTML_EXTRA_STYLESHEET  "${CMAKE_CURRENT_SOURCE_DIR}/docs/doxygen-awesome.css")
    set(DOXYGEN_GENERATE_TAGFILE       "${CMAKE_BINARY_DIR}/docs/source/thrystr.tag")
    doxygen_add_docs(thrystr_source_docs
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ALL  # build on `cmake --build`
        COMMENT "Generating source documentation with Doxygen")
endif()
```

**CI gate** (later): `doxygen --strict` (1.9.7+) flags missing
docstrings — fails the build until coverage is complete. Roll out
gradually: warning → required-on-changed-files → required-everywhere.

### 7.2 User documentation (mdBook)

**Target:** a complete user manual under `docs/user/` written in
Markdown, rendered to a searchable HTML site under
`build/docs/user/book/` on every build. The same Markdown is also
bundled into the application binary (see §7.3).

**Toolchain:** [mdBook](https://rust-lang.github.io/mdBook/) (single Rust
binary, no runtime, ships built-in search). If a hard-no on adding a
Rust toolchain, fall back to `mkdocs` (Python) or a Doxygen "pages"
subsection — mdBook is the recommended option because its on-disk output
is plain HTML+JSON the in-app viewer can also read.

**Manual outline** (`docs/user/src/`):

```
SUMMARY.md              -- mdBook table of contents
introduction.md         -- what thrystr is, what it isn't
installation.md         -- prereqs, building from source
first-steps.md          -- splash, new workspace, open workspace
loading-source.md       -- supported inputs, sliding-window analysis
the-plot-view.md        -- axes, points/lines, max-delta marker, zoom, scroll
sections.md             -- x-line section: create, drag, edit indices
value-mappers.md        -- add/sub/mul/div stack, the wrapping tail
waves.md                -- wave entity, parameters, fit selection
saving-and-loading.md   -- .thryw files, format, portability caveats
keyboard-shortcuts.md
screenshot-mode.md      -- --screenshot, --frames
settings.md             -- overlays, render toggles, zoom defaults
licensing.md            -- non-commercial vs commercial summary
troubleshooting.md
glossary.md
```

**CMake wiring:**

```cmake
find_program(MDBOOK_EXECUTABLE mdbook)
if(MDBOOK_EXECUTABLE)
    add_custom_target(thrystr_user_docs ALL
        COMMAND ${MDBOOK_EXECUTABLE} build
                ${CMAKE_CURRENT_SOURCE_DIR}/docs/user
                --dest-dir ${CMAKE_BINARY_DIR}/docs/user/book
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/docs/user/book.toml
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/docs/user
        COMMENT "Generating user documentation with mdBook")
else()
    message(WARNING "mdbook not found; user docs will be raw Markdown")
endif()
```

### 7.3 In-app documentation viewer

**Target:** a `Help` panel inside the running app that:

- Displays the full user manual rendered inline.
- Supports full-text search across all pages.
- Can be **scoped to the current screen**: pressing `F1` (or clicking a
  `?` chip) opens the panel filtered to docs about the currently active
  view (splash, plot, toolbox, settings, file dialog, mapper stack, etc.).

**Architecture:**

```
+--------------------------------------------------------------+
|  thrystr::gui::docs_panel::DocsPanel                         |
+--------------------------------------------------------------+
|  - corpus :: const DocsCorpus&                               |
|  - search_index :: SearchIndex                               |
|  - active_screen :: ScreenId   (set by parent each frame)    |
|                                                              |
|  Render(state, draw_list) :                                  |
|    - Left rail: TOC, filtered by ScreenId when scoped        |
|    - Right pane: rendered Markdown of selected page          |
|    - Top bar: search input, "All / This screen" toggle       |
+--------------------------------------------------------------+
              ^                                  ^
              |                                  |
              | (cold)                           | (hot)
              |                                  |
+----------------------------+      +---------------------------+
|  DocsCorpus (loaded at     |      |  SearchIndex (built once  |
|  startup from embedded     |      |  from corpus + cached on  |
|  resource blob)            |      |  disk; tiny in-memory)    |
+----------------------------+      +---------------------------+
              ^
              |
+---------------------------------------------------+
|  Build-time embedding:                            |
|  cmake-configure walks docs/user/src/*.md and     |
|  emits docs_resources.cpp with a byte array per   |
|  page + a manifest mapping ScreenId -> page id.   |
+---------------------------------------------------+
```

**Screen-scoped mapping** (`docs/user/screens.toml`):

```toml
[splash]
pages = ["first-steps.md"]

[plot]
pages = ["the-plot-view.md", "sections.md", "keyboard-shortcuts.md"]

[entity_toolbox]
pages = ["waves.md", "sections.md"]

[mapper_stack]
pages = ["value-mappers.md"]

[settings_dialog]
pages = ["settings.md"]

[file_dialog]
pages = ["loading-source.md", "saving-and-loading.md"]
```

**ScreenId injection:** each UI view sets a `state.current_screen =
ScreenId::Plot` (etc.) at the start of its `Render`. The docs panel reads
that to filter the TOC when "This screen" is selected. F1 toggles the
panel open with `scope = ThisScreen`; clicking "All" widens the scope.

**Markdown rendering inside ImGui:**

- Use `imgui_md` (small MIT header) or hand-roll a minimal Markdown
  renderer that handles headings, paragraphs, fenced code,
  inline-code, and bullet lists. The user manual avoids complex
  features (tables, images) so a minimal renderer suffices.
- Code blocks render in the bundled `NotoSansMono`.
- Inline code uses `skald::tokens::surface::input` for the chip
  background.

**Search:**

- Build-time: tokenize each page (strip code fences, headings → boosted
  terms, body terms lowercased) into a simple inverted index. Emit
  `search_index.bin` alongside the embedded markdown.
- Runtime: query → split into tokens → intersect posting lists →
  score by (term frequency × heading-boost) → show top 12 with the
  first matching sentence as a snippet. No external dependency.

**API:**

```cpp
namespace thrystr::gui::docs_panel {

enum class ScreenId {
    None, Splash, Plot, EntityToolbox, MapperStack,
    SettingsDialog, FileDialog, InspectorOverlay, Titlebar,
};

/// Owns the docs corpus + search index. One instance per app.
class DocsPanel {
public:
    DocsPanel();

    /// Render the panel if it's open. Call once per frame.
    void Render(ScreenId current_screen);

    /// Toggle visibility. Called from the help button + F1.
    void Toggle();

    /// Open scoped to the current screen.
    void OpenScopedToCurrent();

private:
    ...
};

}
```

**Resource embedding** (CMake side):

```cmake
add_executable(thrystr_docs_embedder tools/docs_embedder/main.cpp)
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/docs_resources.cpp
    COMMAND $<TARGET_FILE:thrystr_docs_embedder>
        --src   ${CMAKE_CURRENT_SOURCE_DIR}/docs/user/src
        --map   ${CMAKE_CURRENT_SOURCE_DIR}/docs/user/screens.toml
        --out   ${CMAKE_BINARY_DIR}/docs_resources.cpp
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/docs/user/src ...)
target_sources(thrystr_gui PRIVATE ${CMAKE_BINARY_DIR}/docs_resources.cpp)
```

The embedder is itself a tiny C++ tool: reads the markdown tree, emits
`extern const std::array<unsigned char, N> kPageBytes_introduction;` and
a manifest array mapping `ScreenId -> page id list`.

### 7.4 Doc CI gates

- `doxygen --strict`-equivalent (or grep for files without `///` on
  function lines under `src/`) — fails build on missing docstrings.
- `mdbook test` — runs any rust doctests, validates SUMMARY.md links.
- Markdown link-check across the user manual.
- `cmake --build . --target thrystr_source_docs thrystr_user_docs` runs
  on every CI build, with output uploaded as an artifact.

### 7.5 Where the docs end up at runtime

- `build/docs/source/html/` — Doxygen output (developer).
- `build/docs/user/book/` — mdBook output (user-facing static site).
- Embedded copy of the user docs inside the `thrystr` binary —
  consumed by the in-app docs panel.

The runtime never reads from disk — embedding means the docs ship with
the binary and survive packaging into a single executable.

---

## 8. Roll-out checklist (no time estimates)

Order matters; later steps depend on earlier steps.

- [ ] **Replace LICENSE** (§6) — small, isolated, unblocks everything else.
- [ ] **Add `///` docstrings to the existing scalar_analysis API + tests** (§2.4, §3) — the new doc rule should start at the code that already exists.
- [ ] **Backfill core tests** for the gaps in §3.2.
- [ ] **Split `main.cpp`** into `app/` + `gui/` per §4. Stop before vendoring.
- [ ] **Vendor skald** (§5.1).
- [ ] **Vendor ImGui** (§5.2).
- [ ] **Vendor GLFW** (§5.3).
- [ ] **Drop `find_package(glfw3)` and the FetchContent block**; rebuild offline to verify reproducibility (§5.6).
- [ ] **Wire Doxygen target** (§7.1).
- [ ] **Write user manual + wire mdBook target** (§7.2).
- [ ] **Implement docs embedder + in-app DocsPanel + F1 scoping** (§7.3).
- [ ] **Fix the P1 + P2 bugs in §1** as their owning files get touched (avoid drive-by fixes that conflict with the split).
- [ ] **Add CI gates** for docstring coverage, mdBook lint, link check (§7.4).
- [ ] **Tag a release** with the new LICENSE, vendored deps, full docs.

---

## Appendix A — Exact dependency call-site inventory

For the vendoring/encapsulation step (§5), here is the complete set of
external API names referenced in `src/main.cpp`. Anything outside this
list is either vendored already (stb) or a standard library facility.

**GLFW** (must work after `src/gui/vendor/glfw` is in place):

`GLFW_PLATFORM_X11`, `glfwInit`, `glfwInitHint`, `glfwTerminate`,
`glfwSetErrorCallback`, `glfwWindowHint`, `glfwCreateWindow`,
`glfwDestroyWindow`, `glfwGetPlatform`, `glfwGetX11Display`,
`glfwGetX11Window`, `glfwSetWindowAttrib`, `glfwGetWindowAttrib`,
`glfwSetWindowSizeLimits`, `glfwGetMonitorWorkarea`, `glfwGetPrimaryMonitor`,
`glfwGetWindowSize`, `glfwGetWindowPos`, `glfwSetWindowPos`,
`glfwSetWindowSize`, `glfwSwapBuffers`, `glfwSwapInterval`,
`glfwMakeContextCurrent`, `glfwGetFramebufferSize`, `glfwShowWindow`,
`glfwIconifyWindow`, `glfwMaximizeWindow`, `glfwRestoreWindow`,
`glfwPollEvents`, `glfwWindowShouldClose`, `glfwSetWindowShouldClose`,
`glfwGetCursorPos`, `glfwSetCursor`, `glfwCreateStandardCursor`,
`glfwDestroyCursor`, `GLFW_HRESIZE_CURSOR`, `GLFW_VRESIZE_CURSOR`,
`GLFW_RESIZE_NWSE_CURSOR`, `GLFW_RESIZE_NESW_CURSOR`,
`GLFW_DECORATED`, `GLFW_RESIZABLE`, `GLFW_VISIBLE`,
`GLFW_CONTEXT_VERSION_MAJOR`, `GLFW_CONTEXT_VERSION_MINOR`,
`GLFW_ICONIFIED`, `GLFW_MAXIMIZED`, `GLFW_TRUE`, `GLFW_FALSE`,
`GLFW_DONT_CARE`.

**Dear ImGui:**

`ImGui::CreateContext`, `ImGui::DestroyContext`, `ImGui::GetIO`,
`ImGui::NewFrame`, `ImGui::Render`, `ImGui::GetDrawData`,
`ImGui::SetNextWindowPos`, `ImGui::SetNextWindowSize`,
`ImGui::PushStyleVar`, `ImGui::PopStyleVar`,
`ImGui::PushStyleColor`, `ImGui::PopStyleColor`,
`ImGui::Begin`, `ImGui::End`,
`ImGui::BeginPopup`, `ImGui::EndPopup`, `ImGui::OpenPopup`,
`ImGui::BeginPopupModal`, `ImGui::CloseCurrentPopup`,
`ImGui::BeginChild`, `ImGui::EndChild`,
`ImGui::SameLine`, `ImGui::Dummy`,
`ImGui::PushFont`, `ImGui::PopFont`,
`ImGui::TextUnformatted`, `ImGui::TextColored`,
`ImGui::Text`, `ImGui::Selectable`,
`ImGui::InputText`, `ImGui::InputTextWithHint`,
`ImGui::Checkbox`, `ImGui::Combo`, `ImGui::DragScalar`,
`ImGui::SmallButton`, `ImGui::MenuItem`,
`ImGui::CalcTextSize`, `ImGui::GetCursorScreenPos`,
`ImGui::SetCursorScreenPos`, `ImGui::SetCursorPos`,
`ImGui::GetWindowDrawList`, `ImGui::GetItemRectMin/Max`,
`ImGui::GetWindowPos/Size/Width`,
`ImGui::GetContentRegionAvail`, `ImGui::GetScrollX`,
`ImGui::SetScrollX`, `ImGui::GetIO().DisplaySize`,
`ImGui::GetStyle`, `ImGui::PushID`, `ImGui::PopID`,
`ImGui::InvisibleButton`, `ImGui::IsItemActive/Hovered/Clicked`,
`ImGui::IsAnyItemHovered`, `ImGui::IsMouseClicked/Down`,
`ImGui::IsKeyPressed`, `ImGui::SetNextItemWidth`,
`ImGui::BeginTable`, `ImGui::EndTable`, `ImGui::TableNextRow`,
`ImGui::TableSetColumnIndex`, `ImGui::TableSetupColumn`,
`ImGui::TableHeadersRow`, `ImGui::GetColorU32`,
`IM_COL32`, `IM_ARRAYSIZE`, `IMGUI_CHECKVERSION`,
`ImGui_ImplOpenGL3_*`, `ImGui_ImplGlfw_*`.

**OpenGL** (already provided by `OpenGL::GL`):
`glReadPixels`, `glClear`, `glClearColor`, `glViewport`,
`GL_RGBA`, `GL_UNSIGNED_BYTE`, `GL_COLOR_BUFFER_BIT`.

**skald** — see §5.1 for the full surface.

**stb** — `stbi_write_png` (vendored already).

---

*Document scope: a complete code review and roadmap; not an
implementation patch. The implementation steps are sequenced in §8 so
each commit stays small and reviewable.*

---

## 11. Re-review pass — 2026-05-23 (HEAD `3ee3022` + working tree)

This section is the second pass of the code review, taken after the
original baseline `91775d1` and updated with the changes Blake landed
since: PRs #15/#16/#17 + the work-in-progress branch
`feat/playback-keyboard-scrub-speed`.

### 11.1 Baselines for the final pass

| Reference          | SHA / state          | Notes |
|---|---|---|
| Original review    | `91775d1`            | "Set default workspace window geometry" — §1–§10 above |
| **This re-review** | `3ee3022` + uncommitted | "Add lazy x-line playback" merge + WIP scrub speed |
| **Final pass diff baseline** | **`3ee3022`** | Pin this — the final pass will `git diff 3ee3022..HEAD` |
| Branch at review time | `feat/playback-keyboard-scrub-speed` | Working tree had +14/-1 lines in `src/main.cpp` |

`reports/sessions/loki/thrystr-code-review-2026-05-23/baseline.txt`
(committed in the same Vellum bucket as the report) records the exact
working-tree hash for the uncommitted lines so the final pass can
distinguish them from new work.

### 11.2 Resolved findings since `91775d1`

| Original ID | Where it was | Fix landed in | Notes |
|---|---|---|---|
| §1.3 P1 | `CMakeLists.txt:32` — skald pinned to floating `main` | PR #15 (`cb7a1a9` "Address UI review feedback") | Now pinned to commit `73e0887af857b8a9d6c84ffd0fd5119c5e166ec3`. Vendoring (PRs 5-7 in the handoff) supersedes this anyway, but the immediate reproducibility bug is gone. |
| §1.2 P3 (partial) — `save_screenshot` allocates 2× framebuffer | `main.cpp:838-852` baseline | PR #17 indirectly — code moved to `src/render_io.cpp` as `save_screenshot_png` | The double-allocation is still present in the new module; the encapsulation moved but the optimization did not. **Still open**, just relocated. |
| §2.2 (partial) — `draw_titlebar` was 132 lines | `main.cpp:1601` baseline | PR #15 | Now split into `window_control_group_width`, `begin_titlebar_chrome`, `draw_titlebar_background`, `draw_titlebar_wordmark`, plus a smaller `draw_titlebar`. Partial win; still one TU. |
| §2.7 (partial) — `kPi` local constant | `src/scalar_analysis.cpp:20` baseline | PR #17 (transitively via `d834a17`) | Replaced with `std::numbers::pi` (`#include <numbers>`). |
| (new) — screenshot writer extraction toward `gui/` layer | n/a — was inline in `main.cpp` | PR #17 | New `include/thrystr/render_io.hpp` + `src/render_io.cpp` host `Texture2D`, `load_rgba_texture`, `destroy_texture`, `save_screenshot_png`. This is the first chunk of the §4 split executed organically. |

### 11.3 Still open from the original review

Everything else from §1.1, §1.2, §1.3, §2, §3 stands. Specifically the
P1s that have **not** been fixed:

- §1.1 P1 — `scan_highest_entropy_window_stream` decrement-past-stale
  invariant (`src/scalar_analysis.cpp:131-141`) still implicit.
- §1.2 P1 — `load_wave_settings` v1 path still leaks prior-session
  entities (`src/main.cpp:1623-1740` after the rebase; line numbers
  shifted +268 from baseline).
- §1.2 P1 — `.thryw` v1/v2 still host-endian.
- §1.2 P1 — `parse_args` still uses `std::atoi` for `--frames`,
  `--width`, `--height`.

### 11.4 New surface introduced since `91775d1` (must be reviewed before vendoring)

Three substantial subsystems landed; each adds bugs + clean-code debt.

#### 11.4.1 Lazy block cache (PR #17, `src/main.cpp:847-1044`)

The change adds an on-demand block loader so files larger than the
initial best-window slice are still browseable. New types + helpers:
`LazyBlock`, `lazy_block_bytes`, `source_sample_count`,
`clear_lazy_cache`, `read_file_slice`, `find_lazy_block` (×2 const
overloads), `trim_lazy_cache`, `load_lazy_block`,
`seed_lazy_cache_from_analysis`, `ensure_lazy_blocks`, `raw_byte_at`,
`scalar_at`, `byte_from_scalar`, `hex_byte_text`.

| Sev | Where | Finding |
|---|---|---|
| **P1** | `src/main.cpp:871-893` (`read_file_slice`) | Opens a fresh `std::ifstream` per block read. For an Auto-Fit / playback session that walks 1 MiB blocks sequentially over a 1 GiB source, that's 1,024 open/close cycles. Cache one open handle on `AppState` keyed by `source_path`; close on file change or close. |
| **P2** | `src/main.cpp:1046-1050` (`byte_from_scalar`) | Inverse map `std::round((scalar + 1.0f) * 128.0f)` is the asymmetric inverse of `byte_to_scalar`. `byte_to_scalar(0) = -1.0`; `byte_from_scalar(-1.0) = 0` ✓. But `byte_to_scalar(255) = 127/128 = 0.9921875`; `byte_from_scalar(0.9921875) = round(127.5) = 128` — **off by one in the round-to-even direction** on some libcs. Use `std::lround` with explicit cast and clamp. Test: `byte_to_scalar_round_trip_is_identity_for_all_256_values`. |
| **P2** | `src/main.cpp:953-967` (`seed_lazy_cache_from_analysis`) | Does not call `trim_lazy_cache` after the seed push. If the user reloads with a value-mapper change before any other block is loaded, only one block sits in the cache — fine — but the symmetric case (huge `Analysis.bytes`) is not bounded. Call `trim_lazy_cache(state)` at the end. |
| **P2** | `src/main.cpp:996-1018` (`raw_byte_at`) | Fall-through to `state.analysis->bytes` when the cache miss is for the original best-window. That path uses pre-`map_byte_to_wrapped` raw bytes — correct — but `scalar_at` (next function) falls through to `state.analysis->scalars` which **is** wrapped. The two fallback paths therefore use different value-mapper application status. If a user enables a mapper *after* file load and *before* any cache eviction, raw and scalar disagree at the same index. Either make both fall back to raw and recompute or make both fall back to mapped. |
| **P3** | `src/main.cpp:896-913` | `find_lazy_block` non-const + const overload are duplicated logic. Single template or one helper returning index + accessor. |
| **P3** | `src/main.cpp:847-852` (`lazy_block_bytes`) | Hardcoded `1024u * 1024u` MiB conversion; promote `kBytesPerMiB = 1u << 20`. |

#### 11.4.2 X-line playback (PR #17 + uncommitted `feat/playback-keyboard-scrub-speed`)

| Sev | Where | Finding |
|---|---|---|
| **P1** | `src/main.cpp:3032` (committed) | Status line is hardcoded `"x %s / hex %s / 60 pps"`. Uncommitted work added `state.playback_points_per_second` (and the speed presets `kPlaybackSpeedPresets = {12, 24, 30, 60}`) but the status formatter was not updated. After the WIP merges, this text will lie about the actual playback rate. Replace with `"x %s / hex %s / %g pps", state.playback_points_per_second`. |
| **P2** | `src/main.cpp:2875-2894` (`update_xline_playback`) | `playhead_fraction` is incremented by `DeltaTime * points_per_second`. When `points_per_second = 0` (user typed `0` into the custom field), `std::max(0.0, ...)` saves us from negatives, but the function still polls every frame for no reason. Fast-path return when `points_per_second == 0`. |
| **P2** | `src/main.cpp:2806-2843` (uncommitted preset/custom logic) | `kPlaybackSpeedPresets` is `{12, 24, 30, 60}` and the equality test `std::abs(state.playback_points_per_second - preset) < 0.001` decides whether a preset is "active". A user who types `60.0001` into the custom field never highlights the 60 preset. Tighten to integer-equality after `std::lround` for whole presets, or use `< 0.5` for the visual highlight (semantic: "near this preset"). |
| **P2** | `src/main.cpp:2820` (`draw_data_ticker`) | `start = first - (first % stride)` underflows when `first < stride` and `first > 0`? Actually no — when `first < stride`, `first % stride == first`, so `start = 0`. Safe but non-obvious; add comment. |
| **P3** | `src/main.cpp:2967` (uncommitted) | `current_font_size = ImGui::GetFontSize() * kTickerCurrentScale` — the scaled glyph is pushed with `ImGui::SetWindowFontScale` or by selecting a hero font? If using `SetWindowFontScale`, restore to 1.0 after — currently relies on it being a per-window state. Verify. |

#### 11.4.3 Texture / asset loading (PR #15)

| Sev | Where | Finding |
|---|---|---|
| **P2** | `src/render_io.cpp:11-43` (`load_rgba_texture`) | No `glGetError` check after `glTexImage2D`. A texture upload that silently fails leaves `texture.id` set to a valid GL name but with no contents. Add a debug-build error check. |
| **P2** | `CMakeLists.txt` (PR #15 diff) | `THRYSTR_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"` bakes a source-tree path into the binary. Installed binaries can't find assets. Resolve via runtime search: `${exe_dir}/../share/thrystr/assets`, then `${THRYSTR_ASSET_DIR}` as fallback. Mirrors the `THRYSTR_BUNDLED_FONT_DIR` recommendation in handoff §5.1. |
| **P3** | `CMakeLists.txt:86-89` | `thrystr_stb_image_write` library name is now misleading after adding `stb_image_impl.cpp` to it. Rename to `thrystr_stb` (matches handoff §5.4). |
| **P3** | `src/main.cpp` (splash hero init) | No matching `destroy_texture` call wired to splash teardown is obvious from a function-by-function read; verify the splash hero is freed before `glfwTerminate`. |

### 11.5 Clean-code state after the new work

Updated stats:

| Metric | `91775d1` baseline | `3ee3022` + WIP | Delta |
|---|---:|---:|---|
| `src/main.cpp` lines | 3,138 | 3,524 + 14 wip | **+400 (+12.7 %)** — worse |
| Function count in `main.cpp` | 80 | 96 | +16 — including 14 new helpers from the lazy cache + playback work |
| `AppState` member count | 30 | ~42 | **+12** — playback + lazy cache + custom-speed text buffer + playhead drag — even more god-struct |
| Functions ≥ 100 lines | 4 (`draw_plot`, `draw_entity_toolbox`, `draw_titlebar`, `begin_chromed_file_dialog`) | 4 (`draw_plot` grew to ~300+, `draw_entity_toolbox` unchanged, `draw_titlebar` shrank to ~80, `begin_chromed_file_dialog` unchanged) | net 0 but mass redistributed |
| Functions with `///` docstring | 0 | 0 | unchanged — handoff PR 2 still required |

Net: the new functionality is good but the architectural debt grew.
Splitting `main.cpp` (handoff PR 4) is more urgent now than it was at
baseline.

### 11.6 Final-pass directive

The code is still in flight. When Blake signals the next round of UI
+ playback work is committed, the final pass will:

1. Run `git diff 3ee3022..HEAD` to enumerate everything new.
2. Re-walk §11.4's three subsystems for regressions and new bugs.
3. Re-walk §1.1–§1.3 for any pre-existing bug that may have been
   fixed in passing.
4. Re-cost the `main.cpp` line count and update §11.5.
5. Update the handoff `CODEX_HANDOFF_2026-05-23.md` PR table if new
   features need their own PR (e.g. the playback work might need its
   own pre-vendor PR if it ships before the split).
6. Re-record the new baseline SHA in §11.1.

No work happens yet on the §11.4 bugs — they belong in the handoff
plan, not as drive-by fixes during the review pass.

---

*End of re-review §11. The implementation roadmap remains in the
handoff document under `loki/thrystr-context/CODEX_HANDOFF_2026-05-23.md`
and the INDEX at `loki/thrystr-context/INDEX_2026-05-23.md`.*

---

## 12. Final-pass re-review — 2026-05-23 (HEAD `5aaa8b4`)

Third pass. PR #18 ("Add playback scrub and speed controls", merged as
`5aaa8b4`) landed the WIP I captured in §11 plus ~193 additional lines
of scrub + keyboard nav + speed-control extraction.

### 12.1 Baselines

| Reference | SHA | Notes |
|---|---|---|
| Original review | `91775d1` | §1–§10 |
| First re-review | `3ee3022` + WIP | §11 |
| **This pass (final pass to date)** | **`5aaa8b4`** | PR #18 merged to `main`; no WIP at review time |
| Branch at review time | `main` | working tree clean except `CODE_REVIEW.md` |

`reports/sessions/loki/thrystr-code-review-2026-05-23/baseline.txt`
remains the anchor for verifying which of PR #18's lines were already
present in the 2026-05-23 WIP versus newly authored by Blake on top.

### 12.2 Resolved since `3ee3022` (§11 predictions confirmed)

| §11 ID | Where | Fix landed in | Verification |
|---|---|---|---|
| §11.4.2 P1 — `"60 pps"` hardcoded in status line | `src/main.cpp:3032` (pre-merge) | PR #18 / `5aaa8b4` | Status string is now `"x %s / hex %s"`; speed appears in the separate `draw_playback_speed_controls` row. `grep -n '60 pps' src/main.cpp` is empty. |
| §11.4.2 P2 — `update_xline_playback` polls every frame at 0 pps | `src/main.cpp:2875` (pre-merge) | PR #18 / `5aaa8b4` | Now early-returns when `points_per_second <= 0.0` (line 2884). The `state.playback_points_per_second` field is read instead of the old constant. |
| §11.4.2 partial — `toggle_xline_playback` was inline | `src/main.cpp:2894-2898` (pre-merge) | PR #18 / `5aaa8b4` | Extracted as a named free function (line 2791). |
| (clean-code win) — playback subsystem decomposition | n/a | PR #18 / `5aaa8b4` | Six small named helpers added: `set_playhead_index`, `move_playhead_by`, `toggle_xline_playback`, `apply_custom_playback_speed`, `playback_speed_button`, `draw_playback_speed_controls`. Average ~15 lines each. |
| (clean-code win) — ticker font scaling uses explicit font + size | `src/main.cpp:2843` (pre-merge) | PR #18 / `5aaa8b4` | Uses `current_font->CalcTextSizeA(...)` + `draw->AddText(font, size, ...)` directly instead of any `SetWindowFontScale` push/pop dance, so no leak risk. The `state.fonts.mono` fallback is correct. |

### 12.3 Still open from §11 (re-confirmed)

| §11 ID | Where | Status |
|---|---|---|
| §11.4.1 (all six lazy-cache findings — P1/P2/P2/P2/P3/P3) | `src/main.cpp:847-1044` | unchanged — all still present |
| §11.4.2 P2 — preset highlight epsilon `0.001` | now at `src/main.cpp:2839` inside `draw_playback_speed_controls` | unchanged — user-typed `60.0001` still never highlights the 60 preset |
| §11.4.3 (all four texture/asset findings) | `src/render_io.cpp`, `CMakeLists.txt` | unchanged |
| §11.3 — original P1s (streaming max invariant; v1 wave entity leak; host-endian `.thryw`; `atoi` parse) | various | unchanged |

### 12.4 New findings introduced by PR #18

Six new findings, ordered by severity.

#### 12.4.1 P1 — Scrub-claim cancels in-flight selection drag

`src/main.cpp:3118-3128`

```cpp
const float playhead_hit_x =
    plot_left + static_cast<float>(state.playhead_index) * x_step;
const bool playhead_hit =
    std::abs(mouse.x - playhead_hit_x) <= kPlayheadScrubHitPixels &&
    mouse.y >= plot_top &&
    mouse.y <= clip_max.y;
bool playhead_scrub_claimed = state.playhead_dragging;
if (plot_hovered &&
    ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
    playhead_hit) {
    state.playhead_dragging = true;
    state.xline_playing = false;
    state.playhead_fraction = 0.0;
    state.selection_drag_handle = 0;   // <-- nukes selection-drag state
    playhead_scrub_claimed = true;
}
```

If the user is mid-selection-drag (`selection_drag_handle != 0`) and
the playhead happens to be near where they click — common, because
the user often clicks near the section they're inspecting — the
selection drag is silently cancelled. The line is unconditional.

**Fix:** add `&& state.selection_drag_handle == 0` to the scrub-claim
predicate. If the user is already dragging a selection handle, do not
hijack the click. Test:
`scrub_does_not_cancel_active_selection_drag`.

#### 12.4.2 P2 — Arrow-key shortcut leaks through any open popup

`src/main.cpp:3449-3465`

```cpp
void handle_shortcuts(AppState& state) {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput ||
        state.active_dialog != DialogPurpose::None ||
        state.pending_dialog != DialogPurpose::None) {
        return;
    }
    if (state.analysis && source_sample_count(state) > 0u) {
        if (ImGui::IsKeyPressed(ImGuiKey_Space, false)) {
            toggle_xline_playback(state);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, true)) {
            move_playhead_by(state, -1);
        }
        ...
```

`ImGui::CreateContext()` enables `ImGuiConfigFlags_NavEnableKeyboard`
(line 2957 baseline, unchanged). When ImGui is using LeftArrow /
RightArrow for popup navigation, the same key dispatches both ImGui
nav AND `move_playhead_by`. The guard only checks `WantTextInput` and
file dialogs — `##file_menu`, `##create_menu`, `##settings_menu`,
mapper-stack combo popups, and any inspector hover-popup leak.

**Fix:** add `if (ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId)) return;`
to the early-return chain. Test: `arrow_keys_inactive_when_create_menu_open`.

#### 12.4.3 P2 — Scrub hit-radius does not scale with zoom

`src/main.cpp:3105` — `kPlayheadScrubHitPixels = 18.0f` (constant).

At high X-zoom (`x_step > 36 px`) — 18 px hits less than half a sample
slot. Correct.

At default zoom (`x_step ≈ 1 px`) — 18 px covers ~18 sample slots
around the playhead. The user cannot click for selection within ±18
samples of the playhead — those clicks all hijack to scrub. On a
zoomed-out plot of an enwik9 fixture, the dead zone is visible.

At extreme zoom-out (`x_step < 0.1 px`) — 180+ sample dead zone.

**Fix:** scale the hit radius down at low zoom:

```cpp
const float scrub_hit =
    std::min(kPlayheadScrubHitPixels, std::max(2.0f, x_step * 4.0f));
```

Test: `scrub_hit_radius_shrinks_at_low_zoom`.

#### 12.4.4 P2 — `apply_custom_playback_speed` silently rejects bad input

`src/main.cpp:2799-2806`

```cpp
void apply_custom_playback_speed(AppState& state) {
    char* end = nullptr;
    const double value = std::strtod(state.custom_playback_speed_text, &end);
    if (end != state.custom_playback_speed_text &&
        std::isfinite(value) &&
        value > 0.0) {
        state.playback_points_per_second = value;
    }
}
```

The guards are correct — `inf`, `NaN`, `0`, negative, and empty
strings are rejected. But the user gets no feedback when the rejection
fires. Typing `"abc"` leaves the speed at its prior value with no
visual indication the input was discarded.

`strtod` also accepts trailing garbage: `"60abc"` parses as `60.0`
with `end` pointing at `"abc"`. Accepting that is a UX choice — `60`
is what the user "probably meant". Not a bug, but document.

**Fix:** track `apply_custom_playback_speed_failed`; render the
InputText with a red border when set; clear on a successful apply.
Test: `custom_speed_input_signals_parse_failure`.

#### 12.4.5 P2 — `ensure_lazy_blocks(playhead, playhead)` called twice

`src/main.cpp:3023` (added by PR #18) and `src/main.cpp:3139`
(pre-existing).

```cpp
ensure_lazy_blocks(state, state.playhead_index, state.playhead_index);
// ... ~115 lines later ...
ensure_lazy_blocks(state, first, last);
ensure_lazy_blocks(state, state.playhead_index, state.playhead_index);
```

Both calls cover the same block; one suffices. The cost when the
block is cached is a linear scan of `state.lazy_blocks` (≤ 6 entries)
— cheap but wasted. The cost when the block is NOT cached is a full
file-open + read (see §11.4.1 P1, the per-block open-fresh-ifstream
issue) — duplicated.

**Fix:** delete the early call (line 3023). The later pair at line
3138-3139 covers playhead + visible range.

#### 12.4.6 P3 — `draw_playback_speed_controls` missing `ImGui::PushID`

`src/main.cpp:2820-2861`

The four preset buttons render as `"12"`, `"24"`, `"30"`, `"60"` via
`snprintf` with no ImGui ID scope. These string labels happen to be
unique within the current titlebar / plot-top zone, but ImGui
generates widget IDs from the label string by default — if any future
addition adds another button labeled `"60"` in the same window, the
button input handlers fire on whichever is rendered first.

**Fix:** wrap the body in `ImGui::PushID("playback_speed_controls")` /
`ImGui::PopID()`. Defensive; no current bug.

### 12.5 Clean-code state after PR #18

| Metric | `91775d1` | `3ee3022` + WIP | **`5aaa8b4`** | Δ vs original | Δ vs §11 |
|---|---:|---:|---:|---|---|
| `src/main.cpp` lines | 3,138 | 3,538 (3,524 + 14 wip) | **3,700** | +562 (+17.9 %) | +162 |
| `main.cpp` functions | 80 | 96 | **102** | +22 | +6 |
| `AppState` field count | ~30 | ~42 | **~42** | +12 | unchanged (PR #18 added no new fields) |
| Functions ≥ 100 lines | 4 | 4 | 4 | unchanged | unchanged |
| Functions with `///` docstring | 0 | 0 | **0** | unchanged | unchanged |

Verdict on PR #18 specifically:

- **Clean-code win.** Six new helpers, all small and well-named.
  `toggle_xline_playback` / `set_playhead_index` / `move_playhead_by`
  are exactly the kind of helpers the handoff PR 4 split would have
  created. PR #18 effectively pre-cleans the playback subsystem ahead
  of the split.
- **Architectural debt up.** main.cpp +162 lines, +6 functions — the
  PR is net positive on average function size but net negative on file
  size and TU sprawl. The handoff PR 4 is now even more urgent (cost
  to split has barely budged because PR 4 was always a copy-move
  operation; but the surface to verify grew).
- **AppState held steady.** The 4 new playback fields landed in §11's
  WIP; PR #18 added no new fields beyond those.

### 12.6 Updated bug ledger

Cumulative since `91775d1`:

| Bucket | Count | Notes |
|---|---:|---|
| P1 original (§1) | 5 | unchanged |
| P1 newly introduced (§11.4) | 2 | both fixed by PR #18 |
| P1 still open from §11.4 | 0 | (both were §11.4.2's "60 pps" and zero-pps polling) |
| P1 newly introduced this pass (§12.4) | 1 | scrub cancels selection drag |
| **P1 still open total** | **6** | 5 original + 1 from this pass |
| P2 original (§1) | 8 | unchanged |
| P2 newly introduced (§11.4) | 7 | unchanged |
| P2 newly introduced this pass (§12.4) | 4 | popup nav leak, scrub hit radius, custom-speed feedback, dup ensure_lazy_blocks |
| **P2 still open total** | **19** | |
| P3 original (§1) | 8 | unchanged |
| P3 newly introduced (§11.4) | 3 | unchanged |
| P3 newly introduced this pass (§12.4) | 1 | PushID missing on speed controls |
| **P3 still open total** | **12** | |

### 12.7 Recommendations to fold into the handoff PRs

The handoff `CODEX_HANDOFF_2026-05-23.md` PR 4 (the big split) already
authorizes folding in selected bug fixes. Extend its `§6.5` bug-fix
table by:

| Bug | Fix location in PR 4 |
|---|---|
| §12.4.1 P1 — scrub cancels selection drag | Commit 6 (chrome + selection) — extend the latch-grabbed-handle fix |
| §12.4.2 P2 — popup nav leak | Commit 1 (pure helpers — `handle_shortcuts`) — add the popup-open guard |
| §12.4.3 P2 — scrub hit radius | Commit 8 (plot panel split) — `update_selection_from_mouse` helper takes scrub_hit_radius arg |
| §12.4.4 P2 — custom-speed feedback | Commit 8 (mapper-stack split → also covers speed-controls split) |
| §12.4.5 P2 — dup `ensure_lazy_blocks` | Commit 8 (plot panel split — single ensure_lazy_blocks() per frame in the new draw_plot orchestrator) |
| §12.4.6 P3 — PushID missing | Commit 8 (same) |

PR 4 already grew during the WIP/scrub work. Keep the order: PR 0 →
PR 1 (LICENSE, with §24.5 patent+CLA) → PR 2 (docstrings) → PR 3
(tests) → PR 4 (split + bugs above + the 7 original P1/P2 bugs).

### 12.8 Next baseline (for the next pass after this one)

| Field | Value |
|---|---|
| **Final-pass diff anchor for the NEXT review** | **`5aaa8b4`** |
| Branch at this pass | `main` (clean working tree except CODE_REVIEW.md) |
| WIP at this pass | none |
| Anchor file | `reports/sessions/loki/thrystr-code-review-2026-05-23/baseline.txt` keeps the original `3ee3022` + WIP record; a new `baseline-5aaa8b4.txt` is not required because there is no WIP at this baseline. The next pass should run `git diff 5aaa8b4..HEAD`. |

When Blake declares the next round done, the next pass will:

1. `git diff 5aaa8b4..HEAD`.
2. Re-walk PR-by-PR.
3. Append §13 (resolved + still-open + new surface + new bugs +
   clean-code delta + next baseline) to this file.
4. Refresh INDEX §0 with the new SHA.

---

*End of final-pass re-review §12. Implementation roadmap unchanged
from §11 — sequenced in `CODEX_HANDOFF_2026-05-23.md`.*

---

## 13. Re-review pass — 2026-05-23 (HEAD `9314df3`)

Fourth pass. PRs #19 ("Add timeline autoscroll and wheel mode", `0aa1af7`)
and #20 ("Fix timeline autoscroll and wheel scroll mode", `e98af4b`)
merged together as `9314df3`. Net delta: +60/-8 on `src/main.cpp`.

### 13.1 Baselines

| Reference | SHA | Notes |
|---|---|---|
| Original review | `91775d1` | §1–§10 |
| First re-review | `3ee3022` + WIP | §11 |
| Second pass (PR #18) | `5aaa8b4` | §12 |
| **This pass (PRs #19 + #20)** | **`9314df3`** | both merged to `main`; clean working tree |
| Branch at review time | `main` | working tree clean except `CODE_REVIEW.md` |

### 13.2 Resolved since `5aaa8b4`

None of the tracked §1, §11, §12 findings were fixed by this pair of
PRs. (PR #19 introduced a new behavior and PR #20 fixed a regression
in that same new behavior — neither touched any of the pre-existing
bug surface.)

### 13.3 Still open from §1, §11, §12 (re-confirmed)

Every unresolved finding from prior passes remains. Highlights:

| §ID | Where (current line) | Status |
|---|---|---|
| §1.1 P1 — streaming max-delta invariant | `scalar_analysis.cpp:131-141` | unchanged |
| §1.2 P1 — v1 wave-settings entity leak | `main.cpp` (loaded path) | unchanged |
| §1.2 P1 — host-endian `.thryw` | `main.cpp` | unchanged |
| §1.2 P1 — `atoi` arg parsing | `main.cpp:218` | unchanged |
| §11.4.1 (six lazy-cache findings) | `main.cpp:847-1044` | unchanged |
| §11.4.2 P2 — preset epsilon `0.001` | `main.cpp:2846` (was 2839) | unchanged — line shifted +7 by PR #19 inserts |
| §11.4.3 (four texture/asset findings) | `render_io.cpp`, `CMakeLists.txt` | unchanged |
| §12.4.1 P1 — scrub cancels selection drag | `main.cpp:3128` (was 3128) | unchanged — `state.selection_drag_handle = 0;` is still unconditional inside the scrub-claim block |
| §12.4.2 P2 — popup-leak through arrow keys | `main.cpp:3508` (was 3449) | unchanged |
| §12.4.3 P2 — fixed scrub hit radius | `main.cpp:3162` (was 3105) | unchanged |
| §12.4.4 P2 — silent custom-speed reject | `main.cpp:2799-2806` | unchanged |
| §12.4.5 P2 — duplicate `ensure_lazy_blocks` | `main.cpp:3029` + `main.cpp:3207` | unchanged — both calls still present |
| §12.4.6 P3 — missing `PushID` on `playback_speed_button` | `main.cpp:2820-2861` | **worsened** — the helper is now used for SIX buttons in the same window (4 speed presets + wheel-mode + segment-mode); ID collision risk grew |

### 13.4 New findings introduced by PRs #19 + #20

Seven new findings — one P1, four P2, three P3. PR #19 introduced the
two new modes and the autoscroll; PR #20 fixed the regression where
wheel-mode-toggle had stale handling, but neither pass addressed the
finer issues below.

#### 13.4.1 P1 — Autoscroll overrides manual scrolling during playback

`src/main.cpp:3084-3092`

```cpp
if (state.xline_playing) {
    const float child_width = ImGui::GetWindowWidth();
    const float playhead_local =
        margin_left + static_cast<float>(state.playhead_index) * x_step;
    const float visible_midpoint = ImGui::GetScrollX() + child_width * 0.5f;
    if (playhead_local >= visible_midpoint) {
        ImGui::SetScrollX(std::max(0.0f, playhead_local - child_width * 0.5f));
    }
}
```

Every frame during playback the scroll is recomputed to keep the
playhead at-or-behind the visible midpoint. A user who manually
wheel-scrolls or drag-scrolls the timeline during playback (a common
gesture in `wheel_scroll_mode = true`) has the scroll yanked back
within the next frame — they cannot inspect a different region while
playback continues.

**Fix:** track `state.last_manual_scroll_frame`; auto-scroll only
when `(now - last_manual_scroll) > 30 frames` AND the playhead has
actually left the visible window (not just crossed midpoint). This
preserves the "follow the playhead" intent without fighting the user.

Test: `manual_scroll_during_playback_persists_for_30_frames`.

#### 13.4.2 P2 — Two wheel handlers; Shift+wheel silently no-ops in `wheel_scroll_mode`

`src/main.cpp:3087-3094` (child-region handler) and
`src/main.cpp:3122-3140` (plot-canvas handler).

The new child-region handler fires when `child_hovered &&
wheel_scroll_mode && !Ctrl && !Super`. The pre-existing plot-canvas
handler fires when `plot_hovered`. The latter's branches now read:

```cpp
if (io.KeyCtrl || io.KeySuper) {
    // zoom Y
} else if (io.KeyShift && !state.wheel_scroll_mode) {
    // scroll X
} else if (!state.wheel_scroll_mode) {
    // zoom X
}
```

In `wheel_scroll_mode = true`, the plot-canvas handler only fires for
Ctrl/Super (Y zoom). The child-region handler doesn't check Shift.
Net: in `wheel_scroll_mode`, **Shift+wheel does nothing**. User
expectation: shift modifies the wheel axis.

**Fix:** unify into one mode-aware wheel switch. Suggested matrix:

| Modifier | `wheel_scroll_mode = false` | `wheel_scroll_mode = true` |
|---|---|---|
| none | zoom X | scroll X |
| Shift | scroll X | zoom X |
| Ctrl / Super | zoom Y | zoom Y |
| Ctrl + Shift | scroll Y (TBD) | scroll Y (TBD) |

Document the chosen matrix in `keyboard-shortcuts.md` (handoff PR 10).

Test: `wheel_handler_matrix_covers_all_modifier_x_mode_pairs`.

#### 13.4.3 P2 — `playback_speed_button` is now a misnomer

`src/main.cpp:2814-2818` (declaration), called from speed-presets +
wheel-mode + segment-mode buttons.

The helper renders a styled toggle chip. It's no longer
playback-specific. Used for six different button labels in the same
window now.

**Fix:** rename to `mode_chip_button` (or `accent_toggle_button`).
Move it out of the playback section in the file. While renaming, add
`ImGui::PushID(label)` / `ImGui::PopID()` inside the helper so every
call gets its own scope automatically (resolves §12.4.6 P3 in one
move).

#### 13.4.4 P2 — `child_hovered` uses `AllowWhenBlockedByActiveItem`

`src/main.cpp:3081-3082`

```cpp
const bool child_hovered =
    ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
```

This flag means the wheel-scroll handler fires even when another
widget is being actively clicked — including while the user is
scrub-dragging the playhead. Likely intentional ("zoom while
scrubbing") but undocumented. Decide which it is.

**Fix (recommended):** if intentional, add a one-line comment naming
the use case. If unintentional, drop the flag.

#### 13.4.5 P2 — Click-to-jump vs keyboard-scrub: inconsistent view-centering

`src/main.cpp:3251` (click-to-jump) and helper `set_playhead_index`
(`main.cpp:2767-2778`).

```cpp
// click in default (non-segment) mode:
set_playhead_index(state, index, false);              // does NOT center view

// arrow-key scrub via move_playhead_by:
set_playhead_index(state, ..., true);                  // DOES center view
```

A user who scrubs with the keyboard sees the view centered on the new
playhead; a user who clicks elsewhere on the timeline does not. Two
ways to do the same logical action → two behaviors.

**Fix:** pick one rule. Reasonable default: both center. If the click
already lands in the visible region, no-op the center.

Test: `click_and_keyboard_scrub_both_center_view`.

#### 13.4.6 P3 — `selection_drag_handle = 0` written every frame in default mode

`src/main.cpp:3282-3284`

```cpp
} else if (!state.segment_selection_mode) {
    state.selection_drag_handle = 0;
}
```

In the default mode, this branch fires every frame regardless of
whether the handle is already 0. Trivially cheap, but reads as a
defensive write that hides intent.

**Fix:** wrap in `if (state.selection_drag_handle != 0)`. Or delete
entirely and add `state.selection_drag_handle = 0;` inside the
`segment_selection_mode` toggle button handler (already does this at
line 3052).

#### 13.4.7 P3 — Mode toggles styled identically to speed presets

`src/main.cpp:3037-3056`

`"wheel scroll"`/`"wheel scale"` and `"segment"` chips look exactly
like the `"12"`/`"24"`/`"30"`/`"60"` speed presets — same helper,
same colors. Users must read labels carefully. A separator
(`ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical)` or a `Dummy(8.0f,
0)` gap) plus a distinct accent color on mode chips would help
visually segregate "modes" from "values".

#### 13.4.8 P3 — "wheel scale" wording is non-idiomatic

`src/main.cpp:3038`

```cpp
state.wheel_scroll_mode ? "wheel scroll" : "wheel scale"
```

Users say "zoom" not "scale" for what the wheel does. Rename to
`"wheel zoom"`. (The plot already uses `state.zoom_x` / `state.zoom_y`
internally — terminology should match.)

### 13.5 Clean-code state after PRs #19 + #20

| Metric | `91775d1` | `3ee3022`+WIP | `5aaa8b4` | **`9314df3`** | Δ vs original | Δ vs `5aaa8b4` |
|---|---:|---:|---:|---:|---|---|
| `src/main.cpp` lines | 3,138 | 3,538 | 3,700 | **3,772** | +634 (+20.2 %) | +72 |
| `main.cpp` function count | 80 | 96 | 102 | **102** | +22 | 0 |
| `AppState` field count | ~30 | ~42 | ~42 | **~44** | +14 | +2 (wheel_scroll_mode, segment_selection_mode) |
| Functions ≥ 100 lines | 4 | 4 | 4 | 4 | 0 | 0 |
| `///` docstring count | 0 | 0 | 0 | **0** | 0 | 0 |

Net of PRs #19/#20: +72 lines, +2 AppState fields, no new functions
(all changes inline in `draw_plot`'s growing body). The `draw_plot`
function is now ~360 lines (already on the "≥ 100" list) — getting
unwieldy. Handoff PR 4 commit 8 (plot panel split) should aggressively
extract per-mode handlers.

### 13.6 Updated bug ledger

| Bucket | Count | Δ vs §12.6 |
|---|---:|---|
| P1 still open | **7** | +1 (autoscroll-overrides-manual) |
| P2 still open | **22** | +3 (Shift+wheel no-op, helper rename, AllowWhenBlocked flag) |
| P3 still open | **15** | +3 (per-frame handle clear, mode-chip styling, "wheel scale" wording) |
| **Total open findings** | **44** | +7 net introduced by PRs #19 + #20 |

No regressions on previously-resolved findings.

### 13.7 Recommendations to fold into handoff PR 4

Append to the bug-fix table in handoff `§6.5` (already extended in
§12.7 of this review with PR #18's six items):

| Bug | Handoff PR 4 commit |
|---|---|
| §13.4.1 P1 — autoscroll overrides manual | Commit 8 (plot panel split — add `last_manual_scroll_frame` to plot view state) |
| §13.4.2 P2 — Shift+wheel no-op | Commit 8 (single unified wheel switch; matrix in §13.4.2) |
| §13.4.3 P2 — rename `playback_speed_button` | Commit 9 (per-row helper extractions — move to `widgets/mode_chip.{hpp,cpp}` with built-in `PushID`) |
| §13.4.4 P2 — `AllowWhenBlockedByActiveItem` | Commit 8 (decide + document) |
| §13.4.5 P2 — click/keyboard center inconsistency | Commit 1 (pure helpers — `set_playhead_index` default `center_view=true` with caller override; click site passes `true`) |
| §13.4.6 P3 — per-frame handle clear | Commit 6 (chrome + selection — fold into the latch-handle work) |
| §13.4.7 P3 — mode-chip styling | Commit 9 (widget polish) |
| §13.4.8 P3 — "wheel scale" → "wheel zoom" | Commit 9 (label fix) |

### 13.8 Next baseline

| Field | Value |
|---|---|
| **Final-pass diff anchor for the NEXT review** | **`9314df3`** |
| Branch at this pass | `main` (clean working tree except `CODE_REVIEW.md`) |
| WIP at this pass | none |
| Next pass command | `git diff 9314df3..HEAD` |
| Next CODE_REVIEW append | §14 (mirror §11 / §12 / §13 structure) |

---

*End of re-review §13. Implementation roadmap still unchanged —
sequenced in `CODEX_HANDOFF_2026-05-23.md`. PR 4 bug-fix fold list
has grown to 7 (original) + 6 (§12.7) + 8 (§13.7) = 21 bugs to fix
during the big split.*

---

## 14. Re-review pass — 2026-05-23 (HEAD `3a57fa6`)

Fifth pass. PR #21 "Make timeline scroll stateful" (`583c508`, merged
as `3a57fa6`). Net delta: +59 / -28 on `src/main.cpp`; +22 net new
lines.

This is a scroll-refactor PR: replaces `ImGui::GetScrollX()` /
`ImGui::SetScrollX(...)` as the source of truth with a new
`AppState.timeline_scroll_x` field. Restructures the whole scroll
input/output path inside `draw_plot`.

### 14.1 Baselines

| Reference | SHA | Notes |
|---|---|---|
| Original review | `91775d1` | §1–§10 |
| First re-review | `3ee3022` + WIP | §11 |
| Second pass (PR #18) | `5aaa8b4` | §12 |
| Third pass (PRs #19 + #20) | `9314df3` | §13 |
| **This pass (PR #21)** | **`3a57fa6`** | merged to `main`; clean working tree |
| Branch at review time | `main` | working tree clean except `CODE_REVIEW.md` |

### 14.2 Resolved by PR #21

| §ID | Where it was | Resolution |
|---|---|---|
| **§13.4.4 P2** — `child_hovered = IsWindowHovered(AllowWhenBlockedByActiveItem)` | `main.cpp:3081-3082` (in §13 baseline) | **Fixed.** Replaced with an explicit `child_mouse_inside` rectangle test against `io.MousePos` vs `child_pos` + `child_size`. The undocumented "fire-while-blocked" flag is gone; the new check is plain geometry. |
| §13.4.2 P2 — Shift+wheel "silently no-ops" in `wheel_scroll_mode` | `main.cpp:3094` + `main.cpp:3127` | **Correction:** my §13.4.2 description was wrong. The child handler does not check Shift, so Shift+wheel in `wheel_scroll_mode` fires the child handler and scrolls X — same as plain wheel. There is no silent failure; there is a UX *gap* (user might expect Shift to do "the other axis"). **Downgrade from P2 to P3.** Re-cite as "Shift+wheel in wheel_scroll_mode does the same as plain wheel; no orthogonal-axis modifier exists." |

### 14.3 Still open from §1, §11, §12, §13

All other findings persist unchanged. Notable line-number shifts under
the new baseline `3a57fa6`:

| §ID | Original line | Now at |
|---|---|---|
| §11.4.2 P2 — preset epsilon 0.001 | 2846 (§13) | 2846 (unchanged) |
| §12.4.1 P1 — scrub cancels selection drag | 3128 (§13) | 3148 |
| §12.4.5 P2 — duplicate `ensure_lazy_blocks` | 3029 + 3207 (§13) | 3051 + 3229 |
| §12.4.6 P3 — `playback_speed_button` no `PushID` | 2820-2861 (§13) | 2820-2861 (unchanged; helper not touched) |
| §13.4.1 P1 — autoscroll overrides manual scroll | 3084-3092 (§13) | 3118-3128 — see §14.5 for partial mitigation |
| §13.4.5 P2 — click vs keyboard center | 3251 + helper | 3273 + helper |
| §13.4.6 P3 — per-frame handle clear | 3282-3284 (§13) | 3304-3306 |

### 14.4 New findings introduced by PR #21

Five new findings: one P2, four P3. The PR is mostly a sound refactor;
the new findings are second-order issues with the new state-of-truth
pattern.

#### 14.4.1 P2 — `ImGui::SetScrollX(state.timeline_scroll_x)` called four times per frame

`src/main.cpp:3139`, `3141`, `3164`, `3183`

```bash
$ grep -n "SetScrollX" src/main.cpp
3139:    ImGui::SetScrollX(state.timeline_scroll_x);
3141:    ImGui::SetScrollX(state.timeline_scroll_x);
3164:            ImGui::SetScrollX(state.timeline_scroll_x);
3183:            ImGui::SetScrollX(state.timeline_scroll_x);
```

Lines 3139 and 3141 are 2 lines apart — straddling the
`InvisibleButton("##plot_canvas", ...)` call. Lines 3164 and 3183 fire
inside the plot-canvas wheel handler's Shift and zoom-X branches.

ImGui's `SetScrollX` is idempotent within a frame (the last call wins
on the next render), so behaviorally fine. But the call pattern reads
as defensive duplication and obscures intent — a reader has to verify
that `state.timeline_scroll_x` is the value used last.

**Fix:** consolidate to one `ImGui::SetScrollX(state.timeline_scroll_x);`
call at the very end of input handling (just before drawing). Drop the
in-branch calls. Test:
`set_scroll_x_called_at_most_once_per_frame` (asserts via
`std::function` shim or instrumentation).

#### 14.4.2 P3 — `timeline_scroll_x` reset duplicated in three places

`src/main.cpp:240` (struct default) + `main.cpp:1245`
(`open_empty_workspace`) + `main.cpp:1783` (`load_path`).

Each resets the field to `0.0f`. The pattern repeats for
`playhead_index`, `playhead_fraction`, etc. The struct default is
silent; the explicit resets are needed because `AppState` is reused
across workspace loads (not destroyed/recreated).

**Fix:** when PR 4 splits `AppState` per CODE_REVIEW.md §2.6, the
plot-view sub-struct (`PlotViewState`) gets a `reset()` member or is
replaced wholesale on workspace load (preferred). This finding is
resolved as a side effect of PR 4's split.

#### 14.4.3 P3 — Zoom-X branch duplicates `logical_width` computation inline

`src/main.cpp:3175-3179`

```cpp
const float new_logical_width = std::max(
    available.x,
    margin_left + margin_right + static_cast<float>(count - 1) * new_step);
const float new_max_scroll_x = std::max(0.0f, new_logical_width - child_width);
```

This recomputes `logical_width` for the post-zoom step. The pre-zoom
version is computed at the top of `draw_plot` (line ~3072). Two
formulas, same shape — a future change to one will drift.

**Fix:** extract `float compute_logical_width(float available_x,
float margin_left, float margin_right, std::size_t count, float
x_step) noexcept`. Reuse at both sites. Handoff PR 4 commit 8 (plot
panel split) naturally folds this.

#### 14.4.4 P3 — `child_mouse_inside` semantically overlaps `plot_hovered`

`src/main.cpp:3082` and `src/main.cpp:3142`.

```cpp
const bool child_mouse_inside =
    io.MousePos.x >= child_pos.x && ... ;          // rectangle test
// ...
const bool plot_hovered = ImGui::IsItemHovered();   // InvisibleButton
```

`child_mouse_inside` is "inside the scroll-region rect"
(includes ticker + scrollbar). `plot_hovered` is "over the plot
InvisibleButton" (the plot rect only). The two wheel handlers branch
on different conditions for different reasons. Reader has to compare
two predicates to follow the logic.

**Fix:** rename `child_mouse_inside` → `wheel_region_hovered` to make
intent obvious. Add a one-line comment at each test site noting which
sub-region is intended.

#### 14.4.5 P3 — Native scroll-sync race when `io.MouseWheel != 0`

`src/main.cpp:3103-3108`

```cpp
if (!state.pending_scroll_index &&
    !state.xline_playing &&
    io.MouseWheel == 0.0f &&
    std::abs(native_scroll_x - state.timeline_scroll_x) > 0.5f) {
    state.timeline_scroll_x = std::clamp(native_scroll_x, 0.0f, max_scroll_x);
}
```

The `io.MouseWheel == 0.0f` guard exists to prevent picking up
wheel-driven scrolling as if it were a user scrollbar drag. Correct
intent. But: if the user is *both* dragging the scrollbar AND
wheel-scrolling in the same frame (yes, possible with touchpad gestures
or accidental wheel during drag), the sync block exits early and the
wheel-only branch runs — the scrollbar drag is silently dropped that
frame. Recovers next frame when wheel is 0.

**Fix:** acceptable as-is; document the precedence: wheel events
override scrollbar drags within a single frame. One-line comment in
the source.

### 14.5 The §13.4.1 P1 — partial mitigation, fix unchanged

PR #21 added the native scroll-sync block (§14.4.5), which adopts ImGui
scrollbar drags into `timeline_scroll_x` when the app is NOT playing.
This means scrollbar-drag during a paused session is now respected —
previously it would be overwritten by the next `pending_scroll_index`
or autoscroll event.

But during playback (`xline_playing == true`), the autoscroll block at
`main.cpp:3118-3128` still unconditionally snaps the scroll to the
playhead's visible-midpoint position every frame, overriding any
manual scroll. **§13.4.1 P1 stands**, fix per §13.4.1 (add a
`last_manual_scroll_frame` inhibit window). Codex should still fold
this into handoff PR 4 commit 8.

### 14.6 Clean-code state after PR #21

| Metric | `91775d1` | `5aaa8b4` | `9314df3` | **`3a57fa6`** | Δ vs original | Δ vs `9314df3` |
|---|---:|---:|---:|---:|---|---|
| `src/main.cpp` lines | 3,138 | 3,700 | 3,772 | **3,794** | +656 (+20.9 %) | +22 |
| `main.cpp` function count | 80 | 102 | 102 | **102** | +22 | 0 |
| `AppState` field count | ~30 | ~42 | ~44 | **~45** | +15 | +1 (`timeline_scroll_x`) |
| Functions ≥ 100 lines | 4 | 4 | 4 | **4** | unchanged | unchanged |
| `///` docstring count | 0 | 0 | 0 | **0** | 0 | 0 |

`draw_plot` continues to absorb the new state machinery — it's now the
sole owner of all scroll math + autoscroll + native sync. Estimated
~380 lines. PR 4 commit 8 must split it into:

- `compute_plot_geometry(state, available) -> PlotGeometry`
- `sync_native_scroll(state, geometry)`
- `apply_pending_scroll(state, geometry)`
- `apply_autoscroll_during_playback(state, geometry)`
- `handle_plot_wheel(state, geometry, io)`
- `draw_plot_axes(state, geometry, draw)`
- `draw_plot_data(state, geometry, draw)`
- `draw_plot_waves(state, geometry, draw)`
- `draw_plot_chrome(state, geometry)` — speed controls + mode toggles
- `update_selection_from_mouse(state, geometry)`

Then `draw_plot` becomes a ~40-line orchestrator.

### 14.7 Updated bug ledger

| Bucket | After §12 | After §13 | **After §14** | Δ from §13 |
|---|---:|---:|---:|---|
| P1 open | 6 | 7 | **7** | 0 (§14 introduced none) |
| P2 open | 19 | 22 | **22** | **-1 from §13.4.4 fixed, +1 from §14.4.1 → net 0** |
| P3 open | 12 | 15 | **19** | +1 §13.4.2 reclassified, +4 from §14.4.2/3/4/5 |
| **Total open** | **37** | **44** | **48** | +4 net |

Reclassifications:
- §13.4.2 downgraded P2 → P3 (Shift+wheel is a UX gap, not a silent
  failure; corrected understanding).
- §13.4.4 was P2; PR #21 resolved it. Removed from open count.

### 14.8 Recommendations to fold into handoff PR 4

Append to the bug-fix table in handoff `§6.5`. Cumulative fold list
across all reviews (§12.7 + §13.7 + §14.8):

| Bug | Handoff PR 4 commit |
|---|---|
| §14.4.1 P2 — 4× SetScrollX per frame | Commit 8 (plot panel split — consolidate to one call at end of input handling) |
| §14.4.2 P3 — `timeline_scroll_x` reset duplicated | Commit 2 (workspace types — `PlotViewState` is replaced wholesale on workspace load) |
| §14.4.3 P3 — `logical_width` formula duplicated | Commit 8 (extract `compute_logical_width` helper) |
| §14.4.4 P3 — `child_mouse_inside` naming overlap | Commit 8 (rename to `wheel_region_hovered`; add intent comments) |
| §14.4.5 P3 — native-sync wheel precedence | Commit 8 (one-line documentation comment) |

### 14.9 Next baseline

| Field | Value |
|---|---|
| **Final-pass diff anchor for the NEXT review** | **`3a57fa6`** |
| Branch at this pass | `main` (clean working tree except `CODE_REVIEW.md`) |
| WIP at this pass | none |
| Next pass command | `git diff 3a57fa6..HEAD` |
| Next CODE_REVIEW append | §15 (mirror §11 / §12 / §13 / §14 structure) |

---

*End of re-review §14. Implementation roadmap still unchanged —
sequenced in `CODEX_HANDOFF_2026-05-23.md`. PR 4 bug-fix fold list:
7 (original) + 6 (§12.7) + 8 (§13.7) + 5 (§14.8) = **26 bugs** to
fix during the big split. One §13 P2 (#13.4.4 AllowWhenBlocked) was
resolved by PR #21.*

---

## 15. Re-review pass — 2026-05-23 (HEAD `a0dec05`)

Sixth pass. Two PRs merged together:

- **PR #22 "Stabilize playback readout layout"** (`b88fe5c`) — pins
  the playhead readout into a fixed-size `BeginChild` so subsequent
  toolbar widgets stop jumping when the index digit-count changes.
- **PR #23 "Fix timeline scroll snapback"** (`1b0e8c3`) — replaces
  the prior native-scroll-sync guard with explicit detection of "mouse
  is held down inside the bottom strip of the scroll region" (i.e.
  ImGui's horizontal scrollbar area).

Net delta vs `3a57fa6`: +19/−6 on `src/main.cpp` (wc-l net = +2).

### 15.1 Baselines

| Reference | SHA |
|---|---|
| Original review | `91775d1` |
| First re-review | `3ee3022` + WIP |
| Second pass (PR #18) | `5aaa8b4` |
| Third pass (PRs #19 + #20) | `9314df3` |
| Fourth pass (PR #21) | `3a57fa6` |
| **This pass (PRs #22 + #23)** | **`a0dec05`** |
| Branch | `main` (clean working tree except `CODE_REVIEW.md`) |

### 15.2 Resolved by PRs #22 + #23

| §ID | Where it was | Resolution |
|---|---|---|
| **§14.4.5 P3** — native-sync wheel precedence undocumented | `main.cpp:3103-3108` (§14 baseline) | **Fixed.** PR #23 replaces the `io.MouseWheel == 0.0f` heuristic with `native_scrollbar_drag` — an explicit geometric test that the user is left-mouse-clicking in the bottom strip of the scroll-region child. Sync only fires when that test passes. The wheel-vs-scrollbar precedence ambiguity is gone because wheel events no longer trigger sync at all. |

**Implicit fix (undocumented pre-PR-23):** PR #23's title is "Fix
timeline scroll snapback". The bug was almost certainly: after a
wheel-scroll, our `state.timeline_scroll_x` updates and we call
`SetScrollX(...)`, but ImGui's internal scroll state lags by one frame.
On the next frame, the old guard read back the stale `native_scroll_x`,
saw a diff > 0.5, and snapped `state.timeline_scroll_x` back to that
stale value — visually "snapback". The new geometric guard only adopts
on an actual scrollbar drag, so the stale lag never round-trips. This
was a live user-visible bug, not in my §14 ledger. **Noting for
completeness.**

### 15.3 Still open from §1, §11, §12, §13, §14

All other findings persist. Line-number shifts under the new baseline
are minimal (+2 lines net).

| §ID | Status |
|---|---|
| §1 originals (P1×5, P2×8, P3×8) | unchanged |
| §11.4 (lazy cache + texture/asset + preset epsilon) | unchanged |
| §12.4.1 P1 — scrub cancels selection drag | unchanged |
| §12.4.5 P2 — duplicate `ensure_lazy_blocks` | unchanged |
| §12.4.6 P3 — `playback_speed_button` no `PushID` | unchanged |
| §13.4.1 P1 — autoscroll overrides manual scroll | unchanged; see §15.5 for new wrinkle |
| §13.4.x remaining (P2, P3) | unchanged |
| **§14.4.1 P2** — `SetScrollX` called 4× per frame | unchanged; `grep -n SetScrollX` still 4 sites (lines 3141, 3143, 3166, 3185 — shifted +2 lines) |
| §14.4.2 P3 — reset triplication | unchanged |
| §14.4.3 P3 — duplicated `logical_width` | unchanged |
| §14.4.4 P3 — `child_mouse_inside` naming overlap | unchanged; the new `native_scrollbar_drag` adds a **third** user of the same predicate |

### 15.4 New findings introduced by PRs #22 + #23

Five new findings: one P2, four P3.

#### 15.4.1 P2 — `native_scrollbar_drag` geometry uses wrong height constant

`src/main.cpp:3102-3105` (PR #23)

```cpp
const bool native_scrollbar_drag =
    child_mouse_inside &&
    io.MouseDown[ImGuiMouseButton_Left] &&
    io.MousePos.y >= child_pos.y + child_size.y - ImGui::GetFrameHeightWithSpacing();
```

The intent is "mouse is over the horizontal scrollbar at the bottom of
the child." ImGui's actual scrollbar thickness is `ImGui::GetStyle().ScrollbarSize`
(default ~14 px). `GetFrameHeightWithSpacing()` returns the height of a
single-line input/button row (default ~22 px). These differ by ~8 px in
the default theme — and by more on custom themes that adjust
`ItemSpacing`.

**Symptom:** clicks in the bottom ~22 px of the plot canvas (not just
the bottom ~14 px) are treated as scrollbar drags. The native-scroll
adoption then fires every frame the user holds the button. In
practice, `native_scroll_x == state.timeline_scroll_x` in non-drag
frames (because we just set it), so the `> 0.5` diff check skips the
adoption — **benign today**. But the geometry is wrong on principle, and
becomes incorrect immediately if the user ever interleaves a wheel
event with the held click (wheel updates `timeline_scroll_x`; next
frame the held click is still classified as a scrollbar drag; native
adoption snaps back to the lagging native scroll).

**Fix:** use the right constant:

```cpp
const float scrollbar_h = ImGui::GetStyle().ScrollbarSize;
const bool native_scrollbar_drag =
    child_mouse_inside &&
    io.MouseDown[ImGuiMouseButton_Left] &&
    io.MousePos.y >= child_pos.y + child_size.y - scrollbar_h;
```

Test: `scrollbar_drag_detection_uses_scrollbar_size_not_frame_height`.

#### 15.4.2 P3 — `kPlaybackReadoutWidth = 320.0f` hardcoded pixels

`src/main.cpp:58, 3042, 3053`

The fixed width stabilizes the toolbar layout (the whole point of
PR #22), but the chosen 320 px is not derived from font metrics. On a
small-font theme it leaves dead space; on a large-font theme the text
truncates. The companion height (next finding) has the same issue.

**Fix:** derive from font: `const float readout_w = ImGui::CalcTextSize("x 999,999,999  hex FF").x + ImGui::GetStyle().FramePadding.x * 2;`
computed once and cached at file scope or stored on `AppState.fonts`.

#### 15.4.3 P3 — `kPlaybackReadoutHeight = 36.0f` hardcoded pixels

`src/main.cpp:59, 3042`

Same theme-fragility as §15.4.2. The intent is "two text lines of
readout"; should be `2 * ImGui::GetTextLineHeightWithSpacing()` or
similar.

#### 15.4.4 P3 — `BeginChild` used where `BeginGroup` would suffice

`src/main.cpp:3040-3050` (PR #22)

```cpp
ImGui::BeginChild("##playback_readout",
                  ImVec2(kPlaybackReadoutWidth, kPlaybackReadoutHeight),
                  false,
                  ImGuiWindowFlags_NoScrollbar |
                  ImGuiWindowFlags_NoScrollWithMouse);
// ... two TextColored calls ...
ImGui::EndChild();
```

`BeginChild` creates a new ImGui window node — extra state, extra
clipping rect, extra hit-test region. PR #22 needs only a *layout*
anchor of fixed dimensions so the next `SameLine(...)` lands at a
known X. `BeginGroup()` / `EndGroup()` with explicit
`ImGui::Dummy(ImVec2(kPlaybackReadoutWidth, kPlaybackReadoutHeight))`
or `SetCursorPos` accomplishes the same with no window overhead. Or
the existing `SameLine(readout_x + kPlaybackReadoutWidth)` could be
used directly without a child window — the readout text would just be
sized by ImGui as normal, and the `SameLine` would still land at the
fixed X.

**Fix:** `BeginGroup` + explicit `Dummy` for size reservation, or drop
the wrapping entirely and rely on the existing `SameLine(...)` X
offset. Saves one window per frame.

#### 15.4.5 P3 — `native_scrollbar_drag` naming ambiguity

`src/main.cpp:3103`

"Native" is overloaded. In thrystr we use "native" for X11/native
window-manager interaction (see chrome code: `glfwGetX11Window`,
`XMoveResizeWindow`). Here "native" means "ImGui's internal scroll
state". Reader has to figure out from context.

**Fix:** rename to `over_scrollbar_with_mouse_down` or
`scrollbar_drag_in_progress`. Pair with the §14.4.4 rename of
`child_mouse_inside` → `wheel_region_hovered`.

### 15.5 §13.4.1 P1 — the playback override is now MORE jarring

Pre-PR-23, the autoscroll override during playback fought any kind of
scroll change (wheel or scrollbar). Post-PR-23, the new native-sync
adopts scrollbar drags more reliably when *not* playing — but the
autoscroll block still runs unconditionally during playback. So during
playback the user can now:

1. Drag the scrollbar (briefly succeeds — `native_scrollbar_drag` is
   true, sync runs).
2. See the next frame's autoscroll snap the view back to the
   playhead's visible-midpoint.

So during playback the user sees their drag *briefly take effect* and
then disappear. Worse UX than before, when nothing happened at all.

This **does not** change my §13.4.1 fix recommendation
(`last_manual_scroll_frame` inhibit), but it raises the priority of
folding that fix into handoff PR 4 commit 8. **§13.4.1 P1 stays P1**;
note the regression in UX.

### 15.6 Clean-code state after PRs #22 + #23

| Metric | `91775d1` | `9314df3` | `3a57fa6` | **`a0dec05`** | Δ vs original | Δ vs `3a57fa6` |
|---|---:|---:|---:|---:|---|---|
| `src/main.cpp` lines | 3,138 | 3,772 | 3,794 | **3,796** | +658 (+21.0 %) | +2 |
| `main.cpp` function count | 80 | 102 | 102 | **102** | +22 | 0 |
| `AppState` field count | ~30 | ~44 | ~45 | **~45** | +15 | 0 (no new fields this pass) |
| Functions ≥ 100 lines | 4 | 4 | 4 | **4** | unchanged | unchanged |
| `///` docstring count | 0 | 0 | 0 | **0** | 0 | 0 |
| New constants this pass | n/a | n/a | n/a | **+2** | n/a | `kPlaybackReadoutWidth`, `kPlaybackReadoutHeight` |
| New inline ImGui BeginChild | n/a | n/a | n/a | **+1** | n/a | `##playback_readout` |

This pass is a small stabilization PR — barely moves the needle on file
size, no AppState growth, no new helpers. The two findings about
hardcoded readout dimensions and `BeginChild`-vs-`BeginGroup` are minor;
the scrollbar-drag geometry bug is real but currently benign.

### 15.7 Updated bug ledger

| Bucket | After §14 | **After §15** | Δ |
|---|---:|---:|---|
| P1 open | 7 | **7** | 0 (none introduced; none resolved) |
| P2 open | 22 | **23** | +1 (§15.4.1 new) |
| P3 open | 19 | **22** | −1 fixed (§14.4.5), +4 new (§15.4.2/3/4/5) |
| **Total open** | **48** | **52** | +4 net |

### 15.8 Recommendations to fold into handoff PR 4

Append to the bug-fix table in handoff `§6.5`. Cumulative across all
review passes:

| Bug | Handoff PR 4 commit |
|---|---|
| §15.4.1 P2 — scrollbar drag detection uses `GetFrameHeightWithSpacing()` not `ScrollbarSize` | Commit 8 (plot panel split) |
| §15.4.2 P3 — `kPlaybackReadoutWidth` not derived from font | Commit 9 (widget extractions — derive from font metrics) |
| §15.4.3 P3 — `kPlaybackReadoutHeight` not derived from font | Commit 9 |
| §15.4.4 P3 — `BeginChild` where `BeginGroup` would do | Commit 8 (plot panel split — drop the child window for the readout) |
| §15.4.5 P3 — `native_scrollbar_drag` naming | Commit 8 (rename + comment) |

PR 4 §6.5 cumulative bug-fix table count: 7 original + 6 (§12.7) + 8
(§13.7) + 5 (§14.8) + 5 (§15.8) = **31 bugs** to fix during the big
split.

### 15.9 Next baseline

| Field | Value |
|---|---|
| **Next diff anchor** | **`a0dec05`** |
| Branch | `main` (clean working tree except `CODE_REVIEW.md`) |
| WIP | none |
| Next pass command | `git diff a0dec05..HEAD` |
| Next CODE_REVIEW append | §16 (mirror §11/§12/§13/§14/§15 structure) |

---

*End of re-review §15. Implementation roadmap still unchanged.
Six diff passes done since `91775d1`. main.cpp +21.0%, AppState ~45
fields, 52 open findings, 31 bug-fix items for handoff PR 4 to absorb
during the split.*
