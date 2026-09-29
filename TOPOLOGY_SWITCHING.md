# Reset destination engines on activation

> Historical validation record. Commands, output paths and applicable commit IDs
> have been adapted to the standalone repository; the recorded results were not
> rerun during migration unless listed in MIGRATION.md.

Branch: `zed-topology-switch-reset`, from clean fetched master
`2bdaeec8e188fd4a4c710281808b88e25685d21e`. Validation date: 2026-09-19.

## Policy and engine identity

Switch immediately at the existing once-per-block configuration snapshot. If the
selected engine differs from the last active engine, clear the destination's
left and right audio history before processing. Leave the source and other
inactive engines untouched. Returning to an inactive engine always clears it.
There is no crossfade, fade, lookahead, added latency, limiting or background
processing of inactive filters. Ordinary switching clicks are an accepted tradeoff.

| Choice | Stateful engine | Instances |
| --- | --- | --- |
| 0 SVF: LP | `svf` | `svfL`, `svfR` |
| 1 SVF: HP | `svf` | Same SVF instances |
| 2 SVF: BP | `svf` | Same SVF instances |
| 3 SVF: BR | `svf` | Same SVF instances |
| 4 Sallen-Key: LP | `sallenKeyLP` | `korgFilterL.lpf`, `korgFilterR.lpf` |
| 5 Sallen-Key: HP | `sallenKeyHP` | `korgFilterL.hpf`, `korgFilterR.hpf` |
| 6 Transistor ladder: LP | `transistorLadder` | `moogLadderL`, `moogLadderR` |
| 7 Diode ladder: LP | `diodeLadder` | `diodeLadderL`, `diodeLadderR` |

The visible Sallen-Key model contains **two** engines; LP/HP are separate child
implementations with separate pole histories. The four SVF outputs share the same
integrators. Consequently SVF response changes preserve history, while Sallen-Key
LP/HP changes reset the destination child. Repeating a configuration never resets
an already-active engine.

`Engine`, its mapping, `activeEngine` and `resetEngine()` are private processor
implementation details. GUI, parameter callbacks and state serialization do not
access them. `ZDSKmm` adds focused `resetLowPass()`/`resetHighPass()` entry points;
its existing full lifecycle reset is unchanged. All destination resets preserve
sample rate, coefficients, drive and shared smoother trajectories. Both channel
instances are cleared even in mono. The sample loop and its coefficient/smoother
update order are unchanged. No processor-wide reset is called by processing.

The existing lifecycle methods still clear all history and initialize smoothers
as before. Construction starts with `Engine::none`; processor reset invalidates
tracking, and prepare/release reach that reset through their existing calls. The
first processing block resets its already-cleared destination and records it.
An empty block also applies and records an engine change once, without advancing
audio state or smoothers. Subsequent empty blocks with the same engine do not reset
again. State restoration changes parameters only; the next block observes the
restored choice and applies the same engine-identity policy.

The focused helper and mapping use only scalar comparisons and the existing
history-clearing reset methods: no allocation, locking, logging, file access,
GUI/message-thread interaction or host parameter notification. Host lifecycle
calls remain serialized with audio processing, as in the preceding milestone.
No parameter contract, DSP equation, smoothing duration, bus policy, metadata,
GUI behaviour or JUCE revision changed.

## Tests and comparison

`TopologyTests.cpp` extends the optional existing regression executable; normal
builds still default to `ZED_BUILD_TESTS=OFF`. A private friend test seam copies
engines to render expected output without changing the originals. It adds no
public inspection API, global counters, conditional class layouts or runtime
instrumentation. The old regression assertions are unchanged.

The 64 ordered choice pairs each exercise mono and stereo with sine, deterministic
noise, impulse and silence after excitation. Tests pre-excite the destination,
switch away and back, and compare the resulting history/output to an independently
cleared engine copy. They also compare every non-destination engine to its prior
history. Identical-engine transitions preserve that history exactly. The test
explicitly excites right-channel history even in mono, then checks that activation
clears it. Repeated empty blocks and repeated active configurations preserve
non-zero running history. These checks detect both missing and excessive resets.

Stress coverage uses 2,048 blocks per instance at each of 44.1, 48 and 96 kHz,
with four simultaneous processors (mono, left-only stereo, right-only stereo and
silent reference). Five fixed parameter profiles cover ordinary settings,
resonance 1.1, drive 5, low cutoff 12 and high cutoff 120. Block lengths are
0, 1, 7, 64, 257, 3, 0 and 128. Every sample is finite, checked against the bounds
below, and tested for exact channel independence. Every processing call checks
all parameter values and verifies zero parameter/gesture notifications.

Lifecycle tests check construction, preparation, processor reset, rate changes,
release/reprepare and restoration. Restoration must not directly change the
private audio-owned tracking; the next block handles it. Existing channel,
parameter, state, editor and sample-rate/lifecycle suites also run.

### Numerical bounds

Bounds are test assertions, not output limiting or click suppression. They are
conservative bounds derived from the existing equations for the tested fixed
parameter profiles, not subjective click thresholds:

- SVF LP/BP: magnitude at most 1, from `tanh`, plus floating-point margin.
- SVF HP/BR: `|fasttanh| <= 1.25` follows by comparing positive polynomial
  coefficients in the existing rational approximation. The existing SVF cutoff
  cap gives `g < 3.1`; resonance range gives `r` in `[-0.2,1.98]` and denominator
  `D` in `[0.96, 1+3.96*3.1+3.1^2]`. The integrator recurrence has coefficient
  `1/D-1` and `g/D <= 0.625`. These yield the conservative HP/BR bound implemented
  in `outputBound()`, allowing intentional resonance without asserting unity gain.
- For the tested cutoff at most 120 and sample rates at least 44100, Sallen-Key
  poles have `g < 1`: LP gain bound 1, HP absolute impulse-response sum at most 2.
  LP output bound is 2; HP includes division by `k`, with parameter-range minimum
  `k = 2*0.01*0.9/1.1`, giving `2/k`.
- At these cutoffs, transistor ladder poles have LP gain bound 1, preceded by
  `fasttanh` and followed by gain 3.25: bound `1.25*3.25`.
- Diode ladder final `tanh` and gain 10 give magnitude bound 10.

Small floating-point margins are included. These are not hearing-safety guarantees
or an exhaustive proof across arbitrary continuous-parameter automation. Cutoff
135 is outside this focused stress matrix; parameter-range changes are out of scope.

### Recorded results

- Complete Release CTest suite: PASS (6.47 seconds).
- Complete Debug/AddressSanitizer suite: PASS (24.51 seconds), no assertions or
  sanitizer findings. All prior channel/parameter/state/editor/lifecycle tests pass.
- All 64 ordered transitions pass in each build, in both layouts with all four
  input conditions. Reactivation matches cleared-engine output exactly;
  shared-engine and repeated-block histories are retained exactly. Other engines
  remain unchanged. No parameter value/gesture notifications occur in processing.
- All 15 stress rate/profile combinations pass: 122,880 processing blocks across
  the four instances, with exact channel independence. Maximum observed absolute
  output is 1.92974 (48 kHz, cutoff 120, resonance 1.1, drive 5). This is existing
  filter gain/resonance, not a unity-gain or hearing-safety assertion.
- Lifecycle interactions and restored-choice activation pass exactly.
- Fresh tests-OFF Release VST3: PASS, arm64. Normal target has no test source or
  sanitizer options. Bundle ID `com.SouthCoastSynthesis.ZED`, product ZED,
  manufacturer Soco and plug-in code Zedd unchanged. Final ad-hoc signature valid.
- pluginval 1.0.4 strictness 5, seed 12345: SUCCESS (GUI tests enabled).
  The unconfigured external Steinberg validator was skipped, not passed.
- `git diff --check` and artifact/path audit pass. No generated material is tracked.

| Configuration | 44.1 kHz vs master | 48 kHz vs master |
| --- | --- | --- |
| SVF: LP | Byte-identical | Byte-identical |
| SVF: HP | Byte-identical | Byte-identical |
| SVF: BP | Byte-identical | Byte-identical |
| SVF: BR | Byte-identical | Byte-identical |
| Sallen-Key: LP | Byte-identical | Byte-identical |
| Sallen-Key: HP | Byte-identical | Byte-identical |
| Transistor ladder: LP | Byte-identical | Byte-identical |
| Diode ladder: LP | Byte-identical | Byte-identical |

All 273,280 compared floats match byte-for-byte; maximum error 0, no tolerance.
This measures the specified constant-configuration input set, not all possible
signals. The transition history/output tests isolate the intentional difference
to clearing a destination that would otherwise retain stale state. Initial
activation of already-cleared engines and same-engine response changes do not
change history.

## Reproduction commands

Run from the repository root, using fresh directories if these already exist.
All artifacts/logs/tools stay in ignored `build/`. Reuse the clean pinned JUCE
8.0.6 checkout (`51a8a6d7aeae7326956d747737ccf1575e61e209`); omit the source override
to fetch that revision in a fresh clone.

```sh
mkdir -p build/topology-switch-validation/tmp
export TMPDIR="$PWD/build/topology-switch-validation/tmp"
ZED_JUCE_SOURCE="$PWD/build/macos-arm64-release/_deps/juce-src"

cmake -S . -B build/topology-switch-tests-release \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 -DZED_BUILD_TESTS=ON \
  -DFETCHCONTENT_SOURCE_DIR_JUCE="$ZED_JUCE_SOURCE"
cmake --build build/topology-switch-tests-release --target ZEDChannelTests --parallel 4
ctest --test-dir build/topology-switch-tests-release --output-on-failure

cmake -S . -B build/topology-switch-debug-asan \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 -DZED_BUILD_TESTS=ON \
  -DFETCHCONTENT_SOURCE_DIR_JUCE="$ZED_JUCE_SOURCE" \
  -DCMAKE_C_FLAGS='-fsanitize=address -fno-omit-frame-pointer' \
  -DCMAKE_CXX_FLAGS='-fsanitize=address -fno-omit-frame-pointer' \
  -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address
cmake --build build/topology-switch-debug-asan --target ZEDChannelTests --parallel 4
ctest --test-dir build/topology-switch-debug-asan --output-on-failure

cmake -S . -B build/topology-switch-release \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 -DZED_BUILD_TESTS=OFF \
  -DFETCHCONTENT_SOURCE_DIR_JUCE="$ZED_JUCE_SOURCE"
cmake --build build/topology-switch-release --target ZED_VST3 --parallel 4
ZED_BUNDLE="$PWD/build/topology-switch-release/ZED_artefacts/Release/VST3/ZED.vst3"
lipo -archs "$ZED_BUNDLE/Contents/MacOS/ZED"
codesign --verify --deep --strict --verbose=2 "$ZED_BUNDLE"
/usr/libexec/PlistBuddy -c 'Print CFBundleIdentifier' "$ZED_BUNDLE/Contents/Info.plist"
# Reuse the existing official pluginval 1.0.4 app in the new tools directory.
arch -arm64 build/topology-switch-validation/tools/pluginval.app/Contents/MacOS/pluginval \
  --strictness-level 5 --random-seed 12345 \
  --output-dir "$PWD/build/topology-switch-validation" \
  --output-filename pluginval-tests.txt --validate "$ZED_BUNDLE"
git diff --check
```

### Master reference

The immutable source archive was recorded before production edits, then built
independently. No reference source modification is needed: master already has
both dump modes. Every configuration uses the same default parameters, deterministic
sine-plus-impulse input and block sequence `{0,1,17,64,257,512,3,0}` repeated four
times. Each file combines mono, stereo left-only and stereo right-only output:
17,080 floats per configuration and rate, 273,280 floats across both rates.

```sh
mkdir -p build/topology-master-reference
git archive 2bdaeec8e188fd4a4c710281808b88e25685d21e | \
  tar -x -C build/topology-master-reference
cmake -S build/topology-master-reference \
  -B build/topology-master-reference/build \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 -DZED_BUILD_TESTS=ON \
  -DFETCHCONTENT_SOURCE_DIR_JUCE="$ZED_JUCE_SOURCE"
cmake --build build/topology-master-reference/build --target ZEDChannelTests --parallel 4
ZED_MASTER_TEST=build/topology-master-reference/build/ZEDChannelTests_artefacts/Release/ZEDChannelTests
ZED_NEW_TEST=build/topology-switch-tests-release/ZEDChannelTests_artefacts/Release/ZEDChannelTests
"$ZED_MASTER_TEST" --dump-configurations build/topology-master-reference/output-48000
"$ZED_MASTER_TEST" --dump-44100 build/topology-master-reference/output-44100
"$ZED_NEW_TEST" --dump-configurations build/topology-switch-validation/output-48000
"$ZED_NEW_TEST" --dump-44100 build/topology-switch-validation/output-44100
for rate in 44100 48000; do
  for name in 0-0 0-1 0-2 0-3 1-0 1-1 2-0 3-0; do
    cmp "build/topology-master-reference/output-$rate/$name.bin" \
        "build/topology-switch-validation/output-$rate/$name.bin" || exit 1
  done
done
```

## Manual Ableton handoff

The build sets `COPY_PLUGIN_AFTER_BUILD FALSE`; it does not install or overwrite
any plug-in. Test bundle:
`build/topology-switch-release/ZED_artefacts/Release/VST3/ZED.vst3`.
Executable: the bundle's `Contents/MacOS/ZED`.

Quit Live before replacing a test copy. Back up an existing ZED installation,
then copy the **entire bundle** to `~/Library/Audio/Plug-Ins/VST3/`, or use its
containing VST3 directory as Live's custom VST3 folder. Avoid duplicate ZED copies
in scanned locations. Enable/rescan the applicable VST3 folder in Live and load
the new instance. No installation or Live-folder changes were performed here.

The user reports that manual Ableton testing was completed and reviewed before
commit. The exact host layouts and individual cases were not recorded; genuine
1-in/1-out host operation is not independently claimed. The repeatable manual plan is:

1. Use sustained sine/simple material, broadband noise/dense music, and drums.
2. Try every configuration and switch away/back to each engine. Include SVF
   response changes and Sallen-Key LP/HP changes.
3. Try high resonance and drive, slow changes and rapid automation.
4. Check mono and stereo instances, and save/close/reopen the project.

Expect immediate finite clicks to be possible, with no runaway output, unexpected
silence, crash or corrupted state. Returning to an engine should not restore its
old ringing. Non-switching sound should be unchanged. Mono audio on an internally
stereo Ableton track is not evidence of genuine 1-in/1-out operation.

Build warnings remain the existing deprecated Font constructors in
`ZedLookAndFeel.h:139` and `PluginEditor.cpp:163`, plus the intermediate signing
message that is resolved by the unchanged final ad-hoc signing step. No new
compiler warnings or test failures were observed. JUCE editor tests/pluginval
required approved execution outside the application sandbox.

Existing font/signing warnings, licensing, packaging, other formats and unrelated
DSP issues are unchanged. ASan verifies memory safety, not real-time scheduling;
source inspection covers the focused helper's real-time constraints. External
Steinberg validation was not performed; listening validation is user-reported.
