# DSP lifecycle and host sample rate

> Historical validation record. Commands, output paths and applicable commit IDs
> have been adapted to the standalone repository; the recorded results were not
> rerun during migration unless listed in MIGRATION.md.

Branch: `zed-dsp-lifecycle-sample-rate`. Baseline: clean, fetched master
`555d03795b7c8c6022e6f232ad6ad3390d8d6cc9`. Validation: 2026-09-18.
No parameter, configuration ordering, bus policy, GUI, metadata, JUCE revision,
filter equations, per-sample update order or topology-switch policy changes.

## Investigation and scope

Previously `prepareToPlay()` read processor metadata instead of its supplied
rate, initialized filters without clearing their history, assigned several
arbitrary cutoff values (64), and configured two smoothers without initializing
their histories. `releaseResources()` was empty; no processor `reset()` override
existed. Inactive filters retained state through preparation as well as switching.
There were parameter/channel tests but no reset/sample-rate lifecycle tests.

All active sample-rate storage and `init` arguments were `float`. These eight
headers each contained one 44100 default: `ZDSVF`, `ZDSKmm`, `ZDSK`, `ZDSKHPF`,
`ZDML`, `ZDDL`, `ZDOnePole`, `ZDOnePoleEx`. Both `ZDSK` and `ZDSKHPF` also called
`init(44100)` on each of three child poles: six hard-coded child initializations.
All fourteen active-hierarchy literals are removed. Rates are stored/passed as
`double`, with zero denoting not yet initialized rather than a fallback rate.
DSP processing requires preparation with a valid positive host rate.

The Sallen-Key parent already overwrote the child's feedforward coefficient
using its own rate after child initialization. Consequently, finite output or
an ordinary response comparison alone would not detect the child's wrong stored
rate. The new tests inspect every nested rate using narrow private friendship,
without public inspection methods, test-only macros or divergent class layouts.

The entire bundled `jdsplib` was inspected. Five additional 44100 defaults exist
in **unused, uncompiled utilities**: `DelayLin.h` (`sampleRate`), `RevMZ.h`
(`sampleRate`), `voice.h` (`fs`), `oscillatormm.h` (`sr`), `lfo.h` (`sr`). They are
not in ZED's include/construction hierarchy and remain unchanged to avoid work on
unrelated synthesizer/reverb implementations. No 48000 literal or equivalent
fixed-rate initialization was found in the active production hierarchy.

Other unused library components: `EnvGen` has envelope state; `Oscillator` and
`Loscil` have phase/previous-sample/glide state; `Voice` owns those components and
a cached output; `DelayLin` owns a delay vector/index; `CombFilterFB`,
`CombFilterFF`, `CombFilterFBLP` and `DiffusionDelayLin` wrap delays (and a smoother
in FBLP); `RevMini`/`RevMZ` own delay/filter networks and feedback/output state.
`Gain` holds a gain control, not audio history. These are not instantiated by ZED
and are outside this milestone. `DSPMath` is stateless.

## Preparation and reset contract

The host's `prepareToPlay(sampleRate, ...)` argument propagates identically to
both left and right copies of this hierarchy:

- `ZDSVF`: its own rate and SVF coefficient calculation.
- `ZDSKmm` -> `ZDSK` LP -> three `ZDOnePole` children.
- `ZDSKmm` -> `ZDSKHPF` HP -> three `ZDOnePole` children.
- `ZDML` -> four `ZDOnePole` children.
- `ZDDL` -> four `ZDOnePoleEx` children. These extended poles store the rate;
  their rate-dependent coefficients are calculated by the diode parent.
- Shared cutoff/resonance `OnePoleLP` smoothers receive the same double rate.

Existing float audio/coefficient arithmetic is retained. No host rate is
truncated to an integer or silently replaced with 44.1 kHz. Preparation configures
all rates and the 4 Hz smoother coefficients, then calls processor reset. Reset
reinitializes filter controls/coefficient values from current parameters (setting
resonance before cutoff so no old resonance affects the first Sallen-Key sample).
The existing processing loop and its coefficient-update cadence are unchanged.

| Class | History cleared by `reset()` |
| --- | --- |
| `ZDSVF` | `z1`, `z2`, cached LP/HP/BP/BR values |
| `ZDOnePole` | Integrator `z` |
| `ZDOnePoleEx` | Integrator `z` and feedback register |
| `ZDSK`, `ZDSKHPF` | All three child poles |
| `ZDSKmm` | Both LP and HP implementations, regardless of selection |
| `ZDML`, `ZDDL` | All four child poles |
| `OnePoleLP` | `z1`, optionally initialized to a supplied value |
| `DCBlocker` | Previous input/output (`xm1`, `ym1`) |

Filter-level resets retain controls and coefficients. Processor reset clears
**every topology in both channels**, including inactive filters and the two owned
but currently unused DC blockers. It then synchronizes cutoff/resonance smoother
history to their current parameter targets and updates filter coefficients and
drive. It never resets only the selected topology. Switching configurations
without an explicit lifecycle reset continues to retain inactive history.

`reset()` is also safe before first preparation: histories/smoothers are cleared,
but a `dspPrepared` guard prevents coefficient calculation with an unset rate.
`releaseResources()` calls reset to discard tails, then clears the prepared flag.
There are no dynamic DSP resources to free. A later prepare configures everything
again; repeated release/reset is safe. This follows JUCE's reset contract to stop
running tails (pinned JUCE `juce_AudioProcessor.h`). Host lifecycle calls must be
serialized with processing; no additional locks or concurrent-reset guarantee
is introduced. Reset contains no allocation, locks, logging, host notifications,
file operations, GUI calls or message-thread work.

Cutoff and resonance still use their existing 4 Hz one-pole smoothers, each
advanced once per sample. Preparation/reset synchronize to **current parameter
values**, deliberately abandoning any previous trajectory. This also picks up a
parameter change made while audio was stopped. Drive remains unsmoothed. No
host parameter is written by prepare, reset, release or processing.

## Automated coverage and results

Release (5.15 seconds) and Debug/AddressSanitizer (11.75 seconds) full CTest
suites passed, including all existing
parameter, state, editor and mono/stereo tests. Existing assertions are unchanged;
`ChannelLayouts.cpp` only calls the additional suite and adds a 44.1 kHz dump
option (ordinary tests continue to use their original 48 kHz condition).

| Host rate | Eight choices, mono/stereo, finite/non-silent, independent channels | Nested rates, reset, release, state restore |
| --- | --- | --- |
| 44100 | Pass | Pass |
| 48000 | Pass | Pass |
| 88200 | Pass | Pass |
| 96000 | Pass | Pass |
| 192000 | Pass | Pass |
| 48000.123456789 | Pass | Pass (no float/integer rate narrowing) |

The tests deliberately supply stale processor metadata before prepare, proving
that the callback argument is authoritative. Every nested rate and both smoother
coefficients are checked. All topologies are excited before reset; every history
field is checked immediately, including both Sallen-Key branches and both channel
collections. Pending cutoff/resonance changes are synchronized without processing.
Reset and release/reprepare outputs equal fresh processors **exactly**. Silence
after reset equals fresh silence exactly; absolute residual is below `1e-12`,
consistent with existing `tanh(... + 1e-18)` biases rather than stale history.

Repeated preparations cover `44100 -> 96000 -> 48000`,
`96000 -> 44100 -> 192000`, and repeated 192000 preparation, for all choices and
both layouts. Each transition equals a fresh final-rate instance exactly.
Blocks include 0, 1, 7, 64, 257, 512 and 3 samples. Listeners and value snapshots
verify preservation of every host parameter across all lifecycle operations.
State restore followed by prepare equals directly configured fresh processing.
Reset/release before the first preparation also pass without invalid coefficient
calculations or parameter changes.

Normal Release VST3: fresh build passed, executable arm64; bundle identifier
`com.SouthCoastSynthesis.ZED`; product ZED, manufacturer Soco and plug-in Zedd
unchanged. Tests are OFF and no sanitizer flags or test target exist in that
build. Debug ASan instrumentation is confined to its separate build directory.
pluginval 1.0.4 strictness 5, seed 12345: SUCCESS. External Steinberg validator
unavailable/unconfigured; not run. `git diff --check` passes.

## Measured 44.1 kHz master comparison

The master archive and output were recorded before production edits. Its harness
changed only the explicit render rate from 48000 to 44100. Both sides use fresh
processors, default continuous parameters, the same eight choices and identical
sine-plus-impulse input, with blocks `{0,1,17,64,257,512,3,0}` repeated four times.
Each configuration combines genuine mono, stereo left-only and stereo right-only
renders: 17,080 float samples, or 136,640 total. No reference data is tracked.

| Configuration | Fresh output vs master | Maximum absolute difference |
| --- | --- | --- |
| SVF: LP | Different | 0.1350898712 |
| SVF: HP | Different | 0.2089420855 |
| SVF: BP | Different | 0.1329691403 |
| SVF: BR | Different | 0.1692112982 |
| Sallen-Key: LP | Different | 0.3743792251 |
| Sallen-Key: HP | Different | 0.3893865868 |
| Transistor ladder: LP | Different | 0.4691078083 |
| Diode ladder: LP | Different | 0.1179605763 |

These are substantial **intentional startup corrections**, not a claim of
numerical equivalence: master ramps cutoff/resonance from zero, while preparation
now begins at the selected parameters. Sallen-Key also starts with the selected
resonance rather than its constructor value. A separate control experiment
retains master filter equations/rates and changes only those startup values.
**All eight control outputs are byte-identical to the feature output**
(136,640 floats, maximum absolute error 0). This isolates the measured startup
differences to those deliberate initial values. It is not an equivalence claim
for arbitrary parameter trajectories, switching histories or non-44.1 kHz rates.

## Reproduction commands

Run from the repository root; choose new directory names if these already exist.
All generated data, logs and tools belong under ignored `build/`, never Git.
Tools: CMake 4.4.3, Apple Clang 17.0.0 (`clang-1700.6.4.2`), Xcode 26.3
(`17C529`), SDK 26.2, JUCE 8.0.6 commit
`51a8a6d7aeae7326956d747737ccf1575e61e209` (unchanged).

```sh
mkdir -p build/lifecycle-validation/tmp
export TMPDIR="$PWD/build/lifecycle-validation/tmp"
ZED_JUCE_SOURCE="$PWD/build/macos-arm64-release/_deps/juce-src"

cmake -S . -B build/lifecycle-tests-release \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 -DZED_BUILD_TESTS=ON \
  -DFETCHCONTENT_SOURCE_DIR_JUCE="$ZED_JUCE_SOURCE"
cmake --build build/lifecycle-tests-release --target ZEDChannelTests --parallel 4
ctest --test-dir build/lifecycle-tests-release --output-on-failure

cmake -S . -B build/lifecycle-debug-asan \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 -DZED_BUILD_TESTS=ON \
  -DFETCHCONTENT_SOURCE_DIR_JUCE="$ZED_JUCE_SOURCE" \
  -DCMAKE_C_FLAGS='-fsanitize=address -fno-omit-frame-pointer' \
  -DCMAKE_CXX_FLAGS='-fsanitize=address -fno-omit-frame-pointer' \
  -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address
cmake --build build/lifecycle-debug-asan --target ZEDChannelTests --parallel 4
ctest --test-dir build/lifecycle-debug-asan --output-on-failure

cmake -S . -B build/lifecycle-release \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 -DZED_BUILD_TESTS=OFF \
  -DFETCHCONTENT_SOURCE_DIR_JUCE="$ZED_JUCE_SOURCE"
cmake --build build/lifecycle-release --target ZED_VST3 --parallel 4
ZED_BUNDLE="$PWD/build/lifecycle-release/ZED_artefacts/Release/VST3/ZED.vst3"
lipo -archs "$ZED_BUNDLE/Contents/MacOS/ZED"
codesign --verify --deep --strict --verbose=2 "$ZED_BUNDLE"
/usr/libexec/PlistBuddy -c 'Print CFBundleIdentifier' "$ZED_BUNDLE/Contents/Info.plist"
# Reuse the previously verified official pluginval 1.0.4 app, copied into this directory.
arch -arm64 build/lifecycle-validation/tools/pluginval.app/Contents/MacOS/pluginval \
  --strictness-level 5 --random-seed 12345 \
  --output-dir "$PWD/build/lifecycle-validation" \
  --output-filename pluginval-tests.txt --validate "$ZED_BUNDLE"
git diff --check
```

The JUCE source override reuses the existing pinned source checkout; omit it to
fetch the same pinned revision on a fresh clone. Editor tests/pluginval required
approved execution outside the application sandbox. No unrelated tool installation
was performed. Existing build directories were not modified.

### Baseline and comparison

```sh
mkdir -p build/lifecycle-master-reference
git archive 555d03795b7c8c6022e6f232ad6ad3390d8d6cc9 | \
  tar -x -C build/lifecycle-master-reference
python3 - <<'PY'
from pathlib import Path
p = Path('build/lifecycle-master-reference/Tests/ChannelLayouts.cpp')
p.write_text(p.read_text().replace('48000.0', '44100.0'))
PY
cmake -S build/lifecycle-master-reference \
  -B build/lifecycle-master-reference/build \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 -DZED_BUILD_TESTS=ON \
  -DFETCHCONTENT_SOURCE_DIR_JUCE="$ZED_JUCE_SOURCE"
cmake --build build/lifecycle-master-reference/build --target ZEDChannelTests --parallel 4
build/lifecycle-master-reference/build/ZEDChannelTests_artefacts/Release/ZEDChannelTests \
  --dump-configurations build/lifecycle-master-reference/output
build/lifecycle-tests-release/ZEDChannelTests_artefacts/Release/ZEDChannelTests \
  --dump-44100 build/lifecycle-validation/current-output
python3 - <<'PY'
from pathlib import Path
import struct
for p in sorted(Path('build/lifecycle-master-reference/output').glob('*.bin')):
    a = p.read_bytes()
    b = (Path('build/lifecycle-validation/current-output') / p.name).read_bytes()
    assert len(a) == len(b) == 68320
    x, y = (struct.unpack('<17080f', data) for data in (a, b))
    print(p.stem, 'byte-identical' if a == b else 'different', max(abs(i-j) for i,j in zip(x,y)))
PY
```

### Startup-control experiment

This separate copy retains master processing and rate arithmetic. Only smoother
initial values and initial Sallen-Key resonance are changed. The pristine archive,
binary and output remain separate. To recreate the diagnostic copy:

```sh
python3 - <<'PYCONTROL'
from pathlib import Path
import shutil
base=Path('build/lifecycle-master-reference')
control=base/'startup-control'
shutil.copytree(base,control,ignore=shutil.ignore_patterns(
    'startup-control', 'build', 'output', 'startup-control-build', 'startup-control-output'))
p=control/'Source/jdsplib/OnePoleLP.h'
s=p.read_text().replace('public:', 'public:\n    void reset(float value) { z1 = value; }',1)
p.write_text(s)
p=control/'Source/PluginProcessor.cpp';s=p.read_text()
needle='    smootherRes.setCutoff(4.0f, AudioProcessor::getSampleRate());'
s=s.replace(needle,needle+'''
    // CONTROL EXPERIMENT ONLY: retain master equations/rates, correct startup values.
    smootherCutoff.reset(*cutoffParameter);
    smootherRes.reset(*resParameter);
    korgFilterL.setResonance(map(*resParameter, 0.0f, 1.1f, 0.0f, 0.9f));
    korgFilterR.setResonance(map(*resParameter, 0.0f, 1.1f, 0.0f, 0.9f));
''')
p.write_text(s)
PYCONTROL
cmake -S build/lifecycle-master-reference/startup-control \
  -B build/lifecycle-master-reference/startup-control-build \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 -DZED_BUILD_TESTS=ON \
  -DFETCHCONTENT_SOURCE_DIR_JUCE="$ZED_JUCE_SOURCE"
cmake --build build/lifecycle-master-reference/startup-control-build \
  --target ZEDChannelTests --parallel 4
build/lifecycle-master-reference/startup-control-build/ZEDChannelTests_artefacts/Release/ZEDChannelTests \
  --dump-configurations build/lifecycle-master-reference/startup-control-output
for name in 0-0 0-1 0-2 0-3 1-0 1-1 2-0 3-0; do
  cmp "build/lifecycle-master-reference/startup-control-output/$name.bin" \
      "build/lifecycle-validation/current-output/$name.bin" || exit 1
done
```

## Warnings, limitations and manual handoff

- Existing deprecated Font warnings remain in `ZedLookAndFeel.h:139` and
  `PluginEditor.cpp:163`. Existing intermediate signing warning remains; the
  unchanged final ad-hoc signing step produces a valid bundle.
- SDK version querying emitted sandbox FSEvents/cache-directory diagnostics;
  SDK discovery and builds succeeded. These are tool-environment diagnostics.
- Edited DSP headers were CRLF. They were normalized to LF and trailing
  whitespace removed so ordinary `git diff --check` passes. Use
  `git diff --ignore-space-at-eol` to review the small functional changes.
- No testing framework added. Private test friendship has no runtime effect;
  it is defined only in `LifecycleTests.cpp` and introduces no production API.
- ASan checks memory safety, not hard real-time scheduling. The rate/parameter
  matrix is representative, not an exhaustive exploration of resonance extremes.
- Switching clicks, inactive-filter selection policy, drive smoothing, high
  resonance behaviour and other deferred issues remain outside scope.
- **Manual tests have not been performed for this milestone.** Load ZED in
  44.1, 48 and 96 kHz DAW projects; try all eight configurations; stop/restart
  playback; change rate using the host's safe stop/reconfigure workflow; save and
  reopen a project. Check genuine 1-in/1-out and 2-in/2-out instances separately.
  Mono source material on an internally stereo Ableton track does not prove
  genuine mono plug-in operation. Listen for stale tails after host resets and
  confirm selected parameters/GUI survive reload.
