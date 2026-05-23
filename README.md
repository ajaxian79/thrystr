# thrystr

Thrystr is a C++20 Skald/OpenGL/ImGui desktop tool for exploring byte streams as scalar waveforms.

The first target view loads a binary file, finds the sliding 1 MiB window with the largest adjacent-byte delta, maps the bytes into the range `[-1, 1)`, scales X so consecutive slopes stay under a target threshold, and renders the sample with sine/cosine comparison waves.

The plot supports a deterministic value-mapper stack before scalar conversion. Editable stages can add, subtract, multiply, or divide by constants. The fixed tail casts the mapped value into an unsigned modulo-256 byte, then maps that byte into `[-1, 1)`.

## Build

```bash
cmake -B build -S . -DTHRYSTR_SKALD_SOURCE_DIR=/path/to/skald
cmake --build build -j
ctest --test-dir build --output-on-failure
```

If `THRYSTR_SKALD_SOURCE_DIR` is omitted, CMake fetches Skald from GitHub.

## Run

```bash
./build/thrystr --file /path/to/binary
```

In the GUI, use the toolbar to load a source file, load wave settings, or save wave settings. Wave settings are stored in a small binary `.thryw` file and include render toggles, zoom, mapper stack, wave scale, phase-search controls, and the current fitted phases. The wave scale control changes sine/cosine frequency; use `Fit Phases` to sweep phases against the current scale.

For non-interactive rendering:

```bash
./build/thrystr --file /path/to/binary --screenshot out.png --frames 90
```

The app uses GLFW, OpenGL3, Dear ImGui, and the Skald theme/widget kit.
