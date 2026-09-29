# Filter configuration parameter

> Historical validation record. Commands, output paths and applicable commit IDs
> have been adapted to the standalone repository; the recorded results were not
> rerun during migration unless listed in MIGRATION.md.

Implemented on `zed-filter-configuration-parameter`, based on clean, fetched
master `872402d41ba4af644dedab2fc695aa0303ab56c5`. Validation date: 2026-09-18.

The APVTS parameter `filterConfiguration` is a `juce::AudioParameterChoice`.
Its fixed indices are the new preset and automation contract:

| Index / `zed::FilterConfiguration` | Choice | DSP comparison with master |
| --- | --- | --- |
| 0 / `svfLP` | SVF: LP (default) | Byte-identical |
| 1 / `svfHP` | SVF: HP | Byte-identical |
| 2 / `svfBP` | SVF: BP | Byte-identical |
| 3 / `svfBR` | SVF: BR | Byte-identical |
| 4 / `sallenKeyLP` | Sallen-Key: LP | Byte-identical |
| 5 / `sallenKeyHP` | Sallen-Key: HP | Byte-identical |
| 6 / `transistorLadderLP` | Transistor ladder: LP | Byte-identical |
| 7 / `diodeLadderLP` | Diode ladder: LP | Byte-identical |

This intentionally removes `svftype`, `sktype`, `tlftype`, `dlftype`, `lpfmode`,
`hpfmode`, `bpfmode`, and `brfmode`. There are no compatibility parameters,
aliases, or legacy-state migration. Old selector automation/state is unsupported.
The audible startup choice remains SVF: LP. Drive, cutoff and resonance keep
their original IDs, ranges, defaults, reads and smoothing behaviour. Product,
manufacturer and bundle identifiers are unchanged.

## Processing and editor synchronization

`processBlock()` takes one relaxed atomic snapshot of the choice at the existing
per-block selection point. `FilterConfiguration.h` checks finiteness, integral
value and range before converting to the explicit enum; unexpected values use
SVF: LP without repairing the parameter. The selection table yields exactly one
valid model/response. All three previous LP-repair parameter writes are removed,
along with the eight legacy reads and processor-to-GUI selector atomics.

The sample loop is identical to master except for named enum case labels.
Both smoothers still advance once per sample. Coefficient and drive updates,
mono/stereo channel guards, independent filter instances, and active/inactive
sample state are retained. No reset, crossfade, new smoothing, allocation,
locking, logging, listener notification or UI operation is added to processing.
Inspection of `processBlock()` and its ZED helpers finds no host-parameter writes.

The existing model/response buttons retain their labels, tooltips, colours,
dimensions and positions. Model clicks preserve the current response when
supported and otherwise select that model's LP. SVF enables LP/HP/BP/BR;
Sallen-Key enables LP/HP; the ladders enable LP only. Response clicks recheck
the latest model so a stale enabled button cannot overwrite newer automation.

A click makes one `beginChangeGesture` / `setValueNotifyingHost` /
`endChangeGesture` sequence, using `convertTo0to1` for the index. Automatic radio
toggle callbacks are disabled; the parameter-driven refresh sets radio state
with `dontSendNotification`, preventing extra writes from deselected buttons.

The existing 60 Hz JUCE message-thread timer now polls the atomic parameter,
instead of waiting for processor-written status. It refreshes both button groups,
response availability and the filter display even when audio is stopped.
Construction and user clicks also refresh immediately. There are no custom
parameter listeners or cross-thread Component callbacks, and no audio-thread
message posting. Destruction stops the timer; button callbacks are owned by
the editor's buttons. Host automation may take one timer tick to appear.

APVTS serialization/restoration is unchanged and includes the new choice
automatically. No processing callback is needed to repair restored state.

## Automated validation

The existing optional `ZEDChannelTests`/CTest target now also compiles
`Tests/ConfigurationTests.cpp`; no new testing framework was introduced.

- Release: passed (exit 0).
- Debug/AddressSanitizer: passed (exit 0), without JUCE assertions or ASan errors.
- Parameter contract: four parameters total; exact eight-choice ordering,
  SVF: LP default, absence of all eight legacy parameters, unchanged continuous
  ranges/defaults, and defensive conversion of malformed values verified.
- Layout/audio: all 36 layout combinations from disabled/mono/stereo/LCR/quad/5.1
  checked; only matched mono/stereo accepted. All eight choices tested using real
  one-channel and two-channel buffers, finite non-silent output, independent
  left/right state, and empty/variable blocks (0, 1, 17, 64, 257, 512, 3, 0).
- State: each choice survives three successive serialization/restoration cycles
  into fresh processors. Restored output matches a direct parameter selection.
- Switching: every ordered pair of choices repeated eight times for mono and
  stereo, including transitions formerly prone to LP repair. Finite output;
  normalized choice unchanged by processing; zero choice value/gesture listener
  notifications during each process call. This does not assess switching clicks.
- Editor: all eight worker-thread automation and state-restoration cases update
  an open editor without audio processing. Three open/close cycles and a second
  simultaneous instance pass. Actual wired button callbacks satisfy all model
  preservation/fallback and enabled-response rules, with exactly one bracketed
  host notification per click and no feedback from parameter-driven refresh.
  Tests call callbacks on the message thread; physical pointer interaction is
  left for manual checking. A stale BP click after ladder automation is ignored.
- Fresh out-of-tree Release VST3 built with tests OFF; executable is arm64 and
  final strict signature verification passes. Test target absent, no test macros
  or sanitizer flags in normal plug-in compile/link commands. ASan is confined
  to its separate Debug build directory.
- pluginval 1.0.4: strictness 5 SUCCESS (exit 0), seed 12345, GUI tests enabled.
- `git diff --check` passes. Only ZED source, test, build-description and
  documentation files are changed; no artifacts, logs, credentials or local
  machine paths are proposed for tracking.

### Measured DSP comparison

An external `git archive` of master was built separately with the same pinned
JUCE, Apple Clang, Release flags and a dump-only extension to its old test harness.
That harness sets the eight valid legacy float-selector combinations. The new
harness sets the equivalent choice. Filter implementations in the archive were
not modified, and no baseline files were copied into the feature source tree.

For each table row above, both sides rendered mono, stereo left-only and stereo
right-only from fresh instances at 48 kHz, using identical sine-plus-impulse
inputs, block sequence and default continuous parameters. Each render contains
3,416 frames; the three layouts/input arrangements yield 17,080 floats per choice
(68,320 bytes), or 136,640 floats total. **All eight files are byte-identical**;
maximum absolute sample error is 0 for every configuration (no tolerance used).
This is measured equivalence for this input set, not an exhaustive claim for
every continuous-parameter value or switching history.

## Reproduction commands

Run from the repository root. Tested with CMake 4.4.3, Apple Clang 17.0.0
(`clang-1700.6.4.2`), Xcode 26.3, macOS SDK 26.2 and pinned JUCE 8.0.6.
Paths below use placeholders for external temporary directories and pluginval.
Set `ZED_PLUGINVAL` to the executable inside the official pluginval 1.0.4 app.
The JUCE source override reuses the existing clean pinned checkout; omit that
argument on a fresh clone to have CMake fetch it.

```sh
mkdir -p build
ZED_JUCE_SOURCE="$PWD/build/macos-arm64-release/_deps/juce-src"
ZED_VALIDATION_DIR="$(mktemp -d "$PWD/build/zed-configuration-validation.XXXXXXXX")"
ZED_COMPARISON_DIR="$(mktemp -d "$PWD/build/zed-configuration-comparison.XXXXXXXX")"

cmake -S . -B build/channel-release -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
  -DZED_BUILD_TESTS=ON -DFETCHCONTENT_SOURCE_DIR_JUCE="$ZED_JUCE_SOURCE"
cmake --build build/channel-release --target ZEDChannelTests --parallel 4
ctest --test-dir build/channel-release --output-on-failure

cmake -S . -B build/channel-asan -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
  -DZED_BUILD_TESTS=ON -DFETCHCONTENT_SOURCE_DIR_JUCE="$ZED_JUCE_SOURCE" \
  -DCMAKE_C_FLAGS='-fsanitize=address -fno-omit-frame-pointer' \
  -DCMAKE_CXX_FLAGS='-fsanitize=address -fno-omit-frame-pointer' \
  -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address
cmake --build build/channel-asan --target ZEDChannelTests --parallel 4
ctest --test-dir build/channel-asan --output-on-failure

cmake -S . -B "$ZED_VALIDATION_DIR/release" -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
  -DZED_BUILD_TESTS=OFF -DFETCHCONTENT_SOURCE_DIR_JUCE="$ZED_JUCE_SOURCE"
cmake --build "$ZED_VALIDATION_DIR/release" --target ZED_VST3 --parallel 4
ZED_BUNDLE="$ZED_VALIDATION_DIR/release/ZED_artefacts/Release/VST3/ZED.vst3"
lipo -archs "$ZED_BUNDLE/Contents/MacOS/ZED"
codesign --verify --deep --strict --verbose=2 "$ZED_BUNDLE"
arch -arm64 "$ZED_PLUGINVAL" --strictness-level 5 --random-seed 12345 \
  --output-dir "$ZED_VALIDATION_DIR" --output-filename pluginval-tests.txt \
  --validate "$ZED_BUNDLE"
git diff --check
```

For this run the existing configured Release/ASan test trees were rebuilt;
the production and master-comparison build directories were fresh. JUCE editor
tests and pluginval required approved execution outside the application sandbox.
Raw local logs were retained in temporary storage, not added to the repository.

Final pre-commit validation repeated both CTest suites successfully (Release
3.89 seconds; Debug/ASan 4.66 seconds). A fresh production build used
`build/configuration-final-release` instead of the temporary release directory
above, with the same configure options and `ZED_VST3` build command. Its executable
was arm64, strict signature verification passed, and pluginval strictness 5 with
seed 12345 passed. State round-trips for all eight choices, absence of processing
parameter mutation, editor synchronization and mono/stereo regressions all passed
again. The existing `build/ZED.vst3` bundle was not overwritten.

### Recreating the external master harness

```sh
git archive 872402d41ba4af644dedab2fc695aa0303ab56c5 | tar -x -C "$ZED_COMPARISON_DIR"
export ZED_COMPARISON_DIR
python3 - <<'PY'
import os
from pathlib import Path
p = Path(os.environ['ZED_COMPARISON_DIR']) / 'Tests/ChannelLayouts.cpp'
s = p.read_text().replace('#include <fstream>', '#include <fstream>\n#include <filesystem>')
s = s.replace('        require(argc == 1 || dumpStereo, "Usage: ZEDChannelTests [--dump-stereo file]");', '''        const bool dumpConfigurations = argc == 3 && juce::String(argv[1]) == "--dump-configurations";
        require(argc == 1 || dumpStereo || dumpConfigurations, "Invalid arguments");
        if (dumpConfigurations)
            std::filesystem::create_directories(argv[2]);''')
s = s.replace('''                if (dumpStereo)
                {''', '''                if (dumpConfigurations)
                {
                    const auto mono = render(1, topology, mode, 0);
                    std::ofstream file(std::filesystem::path(argv[2]) /
                                       (std::to_string(topology) + "-" + std::to_string(mode) + ".bin"),
                                       std::ios::binary);
                    for (const auto* data : { &mono, &left, &right })
                        file.write(reinterpret_cast<const char*>(data->data()),
                                   static_cast<std::streamsize>(data->size() * sizeof(float)));
                    require(file.good(), "Configuration output write failed");
                }
                else if (dumpStereo)
                {''')
p.write_text(s)
PY
cmake -S "$ZED_COMPARISON_DIR" -B "$ZED_COMPARISON_DIR/build" \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 -DZED_BUILD_TESTS=ON \
  -DFETCHCONTENT_SOURCE_DIR_JUCE="$ZED_JUCE_SOURCE"
cmake --build "$ZED_COMPARISON_DIR/build" --target ZEDChannelTests --parallel 4
"$ZED_COMPARISON_DIR/build/ZEDChannelTests_artefacts/Release/ZEDChannelTests" \
  --dump-configurations "$ZED_COMPARISON_DIR/master-output"
build/channel-release/ZEDChannelTests_artefacts/Release/ZEDChannelTests \
  --dump-configurations "$ZED_COMPARISON_DIR/current-output"
for name in 0-0 0-1 0-2 0-3 1-0 1-1 2-0 3-0; do
  cmp "$ZED_COMPARISON_DIR/master-output/$name.bin" "$ZED_COMPARISON_DIR/current-output/$name.bin" || exit 1
done
```

## Warnings and outstanding manual checks

- Existing deprecated Font constructor warnings remain: `ZedLookAndFeel.h:139`
  and the unchanged label font constructor (now `PluginEditor.cpp:163`).
- Existing intermediate signing diagnostic remains; final ad-hoc signature
  verification succeeds. Signing behaviour was not changed.
- First test compilation failed on four calls to private `AudioParameterChoice`
  overrides. Tests were corrected to use the public `RangedAudioParameter`
  interface; final builds and tests pass. No production workaround was needed.
- pluginval skipped the unconfigured external Steinberg validator. No Steinberg
  validator result is claimed. No other pluginval failures were reported.
- Edited CRLF headers/editor files were normalized to LF and trailing whitespace
  removed; use `git diff --ignore-space-at-eol` to focus on functional changes.
- Manual validation of this parameter change is outstanding: appearance,
  physical button/keyboard interaction and tooltips, host automation recording,
  session save/reload with the editor open/closed, and true mono/stereo DAW
  operation. Earlier Ableton smoke tests do not validate this new parameter
  contract. Nonvisual editor tests verify state/callback logic, not screen pixels.
- Switching clicks, filter lifecycle/sample-rate behaviour, inactive state policy,
  global LookAndFeel, licensing, performance and other deferred issues are not
  addressed. Neither ASan nor the notification tests establish hard-real-time
  scheduling guarantees; source inspection establishes removal of ZED's audio
  parameter mutation and message-thread interactions.
