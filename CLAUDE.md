# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## About Chordify

**Chordify** by **Duphon** is an audio effect plugin (bundle ID `com.duphon.chordify`, manufacturer code `Duph`, plugin code `Chfy`).

- CI/CD via GitHub Actions is disabled for now (push/pull_request triggers commented out in `.github/workflows/`); workflows can still be run manually via `workflow_dispatch`.
- No Intel IPP, no code signing yet.
- CLAP is disabled for now (commented out in `CMakeLists.txt`). Formats built: VST3, AU, AUv3, Standalone.
- The design/roadmap lives in `~/Downloads/Sound-to-Chord Plugin — Implementation Roadmap.md`: a resonator-bank engine (primary) plus an optional FFT spectral engine that imposes chord frequencies on any input. Phase 1 (Faust/SuperCollider prototyping) was skipped in favour of the `Render` harness below.

## About This Project

This project is derived from the [Pamplejuce](https://github.com/sudara/pamplejuce) template — a JUCE audio plugin template using CMake, C++23, and modern CI/CD. It builds cross-platform (macOS, Windows, Linux) with support for multiple plugin formats (VST3, AU, AUv3, CLAP, Standalone).

The template provides the build system, CI/CD, and project structure. The plugin-specific logic lives in `source/`.

## Build Commands

The IDE is CLion. CLI builds share CLion's build directories (`cmake-build-debug` / `cmake-build-release`) and use CLion's bundled Ninja so `.ninja_log` stays compatible between CLI and IDE builds. Default to **Debug**; use Release only if asked (e.g. audio dropouts / high CPU when making music).

```bash
# cmake and ninja aren't on the system PATH — use CLion's bundled copies.
# ninja MUST be on PATH (not just CMAKE_MAKE_PROGRAM): JUCE's VST3 helper runs a nested cmake build that needs it.
export PATH="/Applications/CLion.app/Contents/bin/ninja/mac/aarch64:/Applications/CLion.app/Contents/bin/cmake/mac/aarch64/bin:$PATH"

# Configure (run once, or after CMakeLists.txt changes)
cmake -B cmake-build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build cmake-build-debug

# Run tests (from project root)
ctest --test-dir cmake-build-debug --verbose --output-on-failure

# Or run tests directly
./cmake-build-debug/Tests

# Run a single test by name
./cmake-build-debug/Tests "[test name]"

# Run benchmarks
./cmake-build-debug/Benchmarks
```

For Release, swap `cmake-build-debug` → `cmake-build-release` and `-DCMAKE_BUILD_TYPE=Release`.

On macOS for universal binary: `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"`

## Validation (run after every phase)

```bash
# AU
auval -v aumf Chfy Duph   # aumf = music effect (audio effect that receives MIDI)

# VST3 + AU via pluginval (installed at ~/Applications/pluginval.app)
PV=~/Applications/pluginval.app/Contents/MacOS/pluginval
$PV --strictness-level 10 --validate-in-process --validate ~/Library/Audio/Plug-Ins/VST3/Chordify.vst3
$PV --strictness-level 10 --validate-in-process --validate ~/Library/Audio/Plug-Ins/Components/Chordify.component
```

The AU "Current program is -1" pluginval warning is benign (JUCE AU wrapper).

## Render Harness

`harness/Render.cpp` builds a `Render` CLI that feeds a test signal (noise, pink, impulse, sine) or a WAV file through `PluginProcessor` and writes a 32-bit float WAV. It reports latency, CPU (% of realtime, µs/block), peak/RMS and NaN/inf. Report its CPU/latency numbers on every DSP milestone.

```bash
./cmake-build-debug/Render --input pink --notes 60,64,67 --out /tmp/chord.wav
./cmake-build-debug/Render --list                    # parameter IDs
./cmake-build-debug/Render --set decay=2.5 ...       # real-world parameter values
./cmake-build-debug/Render --preset "Glass Pad" ...  # factory preset (index or name), --set overrides
```

Use a Release build for meaningful CPU numbers.

## Project Structure

- `source/` - Plugin source code (PluginProcessor, PluginEditor)
- `source/params/` - Parameter IDs and `AudioProcessorValueTreeState` layout. IDs and choice-list order are saved in sessions — never rename/reorder, only append
- `tests/` - Catch2 test files
- `source/dsp/` - JUCE-free DSP: `Resonator` (single TPT SVF, reference maths), `ResonatorBank` (the engine), `ChordMapper` (notes → partial grid, voice allocation), `Exciter` (input → envelope-following noise), `Engine` (interface for Phase 4's spectral engine)
- `harness/` - `Render` offline render CLI (see Render Harness)
- `benchmarks/` - Catch2 benchmark files
- `cmake/` - CMake modules (Tests.cmake, Benchmarks.cmake, Assets.cmake, etc.)
- `modules/` - Git submodules: clap-juce-extensions, melatonin_inspector
- `JUCE/` - JUCE framework (git submodule)
- `assets/` - Binary resources (auto-included via juce_add_binary_data)
- `packaging/` - Installer resources and scripts

## Architecture

**Resonator engine** (`source/dsp/`):
- Each partial is a TPT state-variable band-pass (Simper), tuned so the magnitude peak is exactly at f with unity gain and the ring-down is exactly T60: `g = tan(πf/fs)`, `R = 0.001^(1/(T60·fs))`, `k = (1+g²)(1−R²)/(g(1+R²))`. There is no separate Q — Decay sets it.
- `ChordMapper` fills a fixed `PartialGrid` of 8 voices × 16 harmonics (slot = voice·16 + harmonic). Slots keep their index while a voice holds a note, so the bank can glide them. Held partials within 10 cents are merged.
- `ResonatorBank` updates control state every 32 samples (log-frequency glide, ≥ 5 ms; 5 ms amplitude/gate smoothing) and linearly interpolates coefficients across the interval. Active slots are packed into contiguous lanes for auto-vectorisation; silent released slots are switched off.
- `Exciter` (Excite param, default 100%) crossfades the resonator input from the raw signal to pink noise following the input's level (1 ms attack, 30 ms release). Resonators only ring where the input has energy, so a **pitched input only excites the partials matching its own harmonics and the chord collapses to one note** — the noise excitation is what makes every chord tone ring. Don't remove it or lower its default without re-testing pitched input (`tests/ParametersTests.cpp` "[chord]").
- Chord sources: Internal (root + chord type params), MIDI (one chord tone per held key), MIDI Root (last held key is the root, chord type builds on it).
- Wet gain = `makeupGain` (+25 dB: drums/voice/pads land within ~2 dB of their input level at defaults, pink noise ~3 dB under) × `sqrt(T60)` decay compensation × `1/sqrt(Σ held amplitude²)`, then a soft limiter above −1 dBFS.
- Signal chain: mono input sum → `InputHighPass` → `Exciter` → `ResonatorBank` → `TiltEq` (Tone) → dry/wet Mix → Output gain. HPF and Tone affect only the wet path.
- Factory presets live in `source/params/Presets.cpp` and are exposed as host programs; each resets unlisted parameters to defaults. UI code is in `source/ui/` (Theme = LookAndFeel + colours, Widgets). The editor must call `setLookAndFeel` *after* adding its children, or sliders keep default-styled value boxes.
- The processor renders between MIDI events (sample-accurate chord changes), excites the bank with the mono input sum, and recomputes partials only when their inputs change.

**SharedCode Library**: The `SharedCode` INTERFACE library links plugin source code to both the main plugin target and the Tests target, avoiding ODR violations.

**CMake Modules**:
- `PamplejuceVersion.cmake` - Reads VERSION file, optional auto-bump patch level
- `PamplejuceFunctions.cmake` - One include that defines the `pamplejuce_*` functions below (and turns off their legacy include-time behavior)
- `PamplejuceIPP.cmake` - Intel IPP integration (optional)

Target setup happens via explicit function calls in `CMakeLists.txt`, not at include time:
- `pamplejuce_add_assets()` - Includes all files in assets/ as binary data (from `Assets.cmake`)
- `pamplejuce_add_tests()` - Configures the Catch2 test target (from `Tests.cmake`)
- `pamplejuce_add_benchmarks()` - Configures the Catch2 benchmark target (from `Benchmarks.cmake`)
- `pamplejuce_shared_code_defaults()` - C++23, fast math (from `SharedCodeDefaults.cmake`)
- `pamplejuce_xcode_prettify()` - Xcode folder/scheme cleanup (from `XcodePrettify.cmake`)

Note: after `include(PamplejuceFunctions)`, a plain `include(Tests)` (or Assets, etc.) is a no-op - call the function instead.

**Test Discovery**: Uses `catch_discover_tests()` with `PRE_TEST` discovery mode for Xcode compatibility.

## Key Configuration

Edit `CMakeLists.txt` to customize:
- `PROJECT_NAME` - Internal name (no spaces)
- `PRODUCT_NAME` - Display name in DAWs (can have spaces)
- `COMPANY_NAME` - Used for bundle name
- `BUNDLE_ID` - macOS bundle identifier
- `FORMATS` - Plugin formats to build (Standalone AU VST3 AUv3)
- `PLUGIN_MANUFACTURER_CODE` / `PLUGIN_CODE` - 4-character plugin IDs

Version is read from the `VERSION` file in project root.

## Code Quality

Always resolve any compile warnings encountered during builds. Warnings should be treated as errors and fixed before considering a task complete.

Note: LSP/clangd often reports false positive diagnostic errors (like "undeclared identifier", "file not found") because it doesn't have full context of the JUCE module system. Ignore these unless the actual build fails.

## Includes

JUCE modules include common standard library headers (`<vector>`, `<algorithm>`, `<string>`, `<memory>`, etc.) so you don't need to add those explicitly in JUCE code. Adding them is harmless but redundant.

## Threading Model

JUCE plugins have two main threads:

- **Audio thread**: Runs `processBlock` — must be realtime-safe (see below). Never block, allocate, or lock.
- **Message thread**: Runs UI callbacks, parameter listeners, and timer callbacks. Owns the `MessageManager`.

To communicate between them:
- **Simple values**: Use `std::atomic` or JUCE's `AudioParameterFloat`/`AudioParameterBool` (which are atomic under the hood)
- **Larger data**: Use a lock-free queue (e.g. `moodycamel::ReaderWriterQueue`) to pass data from message → audio thread
- **Audio → UI updates**: Use `juce::AsyncUpdater` or `juce::Timer` on the message thread to poll state — never call UI code from the audio thread

## Realtime Safety

For anything in the audio thread / hot DSP path (e.g. `processBlock`):
- Allocate in constructors or `prepareToPlay`, not while rendering audio
- Avoid dynamic allocations and container growth (`std::vector::push_back`, map insertion, string building)
- Prefer fixed-size storage (`std::array`, preallocated buffers, fixed-capacity queues)
- Keep operations deterministic and lock-free where possible

## Adding Dependencies

**JUCE Modules** live in `modules/` as git submodules. Add with `git submodule add`, then `add_subdirectory` and link to `SharedCode` in `CMakeLists.txt`. Some useful ones:

- [melatonin_inspector](https://github.com/sudara/melatonin_inspector) — runtime component debugger (already included)
- [melatonin_blur](https://github.com/sudara/melatonin_blur) — fast cross-platform blurs for C++ UI (shadows, glows, frosted glass)
- [melatonin_perfetto](https://github.com/sudara/melatonin_perfetto) — performance tracing with Perfetto, great for profiling `processBlock` and paint calls
- [gin](https://github.com/FigBug/gin) — large collection of utilities (DSP, UI components, LookAndFeel, etc.)

**Non-JUCE C++ libraries** should be added via [CPM.cmake](https://github.com/cpm-cmake/CPM.cmake) which is already configured. CPM downloads and caches dependencies at configure time — no submodule needed:

```cmake
CPMAddPackage("gh:nlohmann/json@3.11.3")
target_link_libraries(SharedCode INTERFACE nlohmann_json::nlohmann_json)
```

Some useful CPM libraries:
- [nlohmann/json](https://github.com/nlohmann/json) — JSON parsing/serialization
- [cameron314/readerwriterqueue](https://github.com/cameron314/readerwriterqueue) — lock-free single-producer/single-consumer queue, ideal for audio↔message thread communication

## Code Style

Uses `.clang-format` with Allman-style braces, 4-space indentation, no column limit.
