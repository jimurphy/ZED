# Mono/stereo correctness validation — 2026-09-18

Branch: `zed-mono-stereo-support`, created from fetched, up-to-date master
`4ba0198927765a4feb6604ffe986f60bab0f0953`.

## Cause and changes

The processor advertised mono but unconditionally obtained, read, and wrote
buffer channel 1. A genuine one-channel buffer cannot supply that channel.
The existing constructor defaults to stereo and owns separate left/right SVF,
Sallen-Key, transistor-ladder, and diode-ladder filters, with shared cutoff and
resonance smoothers.

- `Source/PluginProcessor.cpp`: explicitly require matching mono or matching
  stereo main buses. Only obtain channel 1 when it is active and present; guard
  its four topology-specific DSP calls and output write. Mono runs through the
  existing left state without duplication or downmixing. Stereo retains both
  independent filter banks and the original order of operations.
- `CMakeLists.txt`: add optional `ZED_BUILD_TESTS` (default OFF), a small native
  executable, and one CTest entry. The production VST3 target remains a non-synth
  audio effect without a preferred-channel override; its channel configuration
  agrees with the processor. No production build settings or identifiers change.
- `Tests/ChannelLayouts.cpp`: standalone regression test using JUCE and the
  actual processor; no additional testing framework. Its optional stereo-dump
  mode supports comparison with the original master implementation.
- `CHANNEL_VALIDATION.md`: this report and reproduction commands.

All parameter reads, IDs, ranges, topology selection, equations, coefficient
updates, and smoothing constants are unchanged. Both smoothers still advance
exactly once per sample; left and right coefficient updates remain in their
original positions, including in mono. Right-channel filter sample processing
is skipped in mono. No allocations, locks, logging, or UI calls were added to
`processBlock()`. Its existing parameter mutation is deliberately untouched.
No lifecycle, switching, performance, GUI, or licensing changes were made.

## Automated results

| Check | Result |
| --- | --- |
| Main bus layouts | Only mono→mono and stereo→stereo accepted |
| Rejection matrix | All other pairs from disabled, mono, stereo, LCR, quad, and 5.1 rejected (36 pairs tested via both support query and `setBusesLayout`) |
| Release channel test | Passed, exit 0 |
| Debug + AddressSanitizer channel test | Passed, exit 0; no JUCE assertions or ASan errors |
| Release VST3 | Built; `lipo -archs` reports `arm64`; strict signature verification passes |
| pluginval 1.0.4, strictness 5 | SUCCESS, exit 0, seed 12345; GUI tests enabled |
| Stereo comparison with master | Byte-identical for the measured test set |

For each render the test creates a fresh processor, configures the layout, sets
the host rate/block-size details, and calls `prepareToPlay(48000, 512)` before
processing. Mono uses a real `AudioBuffer<float>(1, sampleCount)`, never a stereo
buffer carrying mono material. Deterministic sine-plus-impulse input exercises:

- All four topologies: SVF LP/HP/BP/BR, Sallen-Key LP/HP, and both ladder LP modes.
- Block sizes `0, 1, 17, 64, 257, 512, 3, 0`, repeated four times, including empty
  blocks before and after audio. Each render contains 3,416 audio frames.
- Finite output, non-silent response, unchanged channel count, exact equality
  between mono and the corresponding independent stereo channel, and isolation
  of the other channel against a separate mono silence render. The silence
  reference preserves the existing SVF's tiny nonlinear offsets.

The ASan executable links `libclang_rt.asan_osx_dynamic.dylib`; the processor and
JUCE were rebuilt with instrumentation and Debug assertions. This is address
checking, not a claim of leak, real-time safety, or thread-safety validation.

Stereo equivalence was **measured**, not merely inferred. The original master
processor was recompiled with the same Release flags and substituted into a copy
of the current shared-code archive. Both executables used the same harness and
other objects. Left-only and right-only renders across the eight topology/mode
combinations produced 109,312 floats (437,248 bytes) per executable. `cmp` returned
0; both SHA-256 hashes were:

```text
f27ee633e82ab396cd432c7a9e3b6311f1ea34be7285327cb6dbc1e33742c18b
```

This establishes equivalence for those deterministic inputs at default drive,
cutoff, and resonance, not for all parameter trajectories or host sequences.

## Commands

Final integration checks repeated successfully using the normal production
preset (`ZED_BUILD_TESTS=OFF`), separately from the test and ASan build trees.
The normal build has no regression-test target or sanitizer/test compile flags.
The final stereo dump still matches the original master dump byte for byte.

```sh
(cd ZED && cmake --preset macos-arm64-release && cmake --build --preset macos-arm64-release --parallel 4)
cmake --build ZED/build/channel-release --target ZEDChannelTests --parallel 4
cmake --build ZED/build/channel-asan --target ZEDChannelTests --parallel 4
ctest --test-dir ZED/build/channel-release --output-on-failure
ctest --test-dir ZED/build/channel-asan --output-on-failure
ZED/build/channel-release/ZEDChannelTests_artefacts/Release/ZEDChannelTests \
  --dump-stereo ZED/build/channel-release/stereo-reference/current.bin
cmp ZED/build/channel-release/stereo-reference/master.bin ZED/build/channel-release/stereo-reference/current.bin
/usr/bin/arch -arm64 "$ZED_PLUGINVAL" --strictness-level 5 --random-seed 12345 \
  --output-dir /tmp --output-filename zed-integration-pluginval-tests.txt \
  --validate "$PWD/ZED/build/macos-arm64-release/ZED_artefacts/Release/VST3/ZED.vst3"
lipo -archs ZED/build/macos-arm64-release/ZED_artefacts/Release/VST3/ZED.vst3/Contents/MacOS/ZED
codesign --verify --deep --strict --verbose=2 ZED/build/macos-arm64-release/ZED_artefacts/Release/VST3/ZED.vst3
git diff --check
```

The final production bundle is
`build/macos-arm64-release/ZED_artefacts/Release/VST3/ZED.vst3` relative to ZED.
Existing machine-specific paths in the retained Projucer/IDE files predate this
change and remain untouched; none are introduced in these four changed files.

Run from the repository root. CMake 4.4.3, Apple Clang 17.0.0
(`clang-1700.6.4.2`), JUCE 8.0.6 at the existing pinned commit. These builds reuse
only the pinned JUCE source fetched by the earlier baseline build. On a clean
checkout, omit the `FETCHCONTENT_SOURCE_DIR_JUCE` argument to fetch the same pin.

```sh
cmake -S ZED -B ZED/build/channel-release \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 -DZED_BUILD_TESTS=ON \
  -DFETCHCONTENT_SOURCE_DIR_JUCE="$PWD/ZED/build/macos-arm64-release/_deps/juce-src" \
  > /tmp/zed-channel-configure.log 2>&1
cmake --build ZED/build/channel-release --target ZEDChannelTests ZED_VST3 --parallel 4 \
  > /tmp/zed-channel-build.log 2>&1
# After correcting the test target and silence-reference assertion:
cmake --build ZED/build/channel-release --target ZEDChannelTests --parallel 4 \
  > /tmp/zed-channel-test-final-build.log 2>&1
ctest --test-dir ZED/build/channel-release --output-on-failure \
  > /tmp/zed-channel-release-test-final.log 2>&1

cmake -S ZED -B ZED/build/channel-asan \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 -DZED_BUILD_TESTS=ON \
  -DCMAKE_C_FLAGS='-fsanitize=address -fno-omit-frame-pointer' \
  -DCMAKE_CXX_FLAGS='-fsanitize=address -fno-omit-frame-pointer' \
  -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address \
  -DFETCHCONTENT_SOURCE_DIR_JUCE="$PWD/ZED/build/macos-arm64-release/_deps/juce-src" \
  > /tmp/zed-channel-asan-configure.log 2>&1
cmake --build ZED/build/channel-asan --target ZEDChannelTests --parallel 4 \
  > /tmp/zed-channel-asan-build.log 2>&1
# After correcting the silence-reference assertion:
cmake --build ZED/build/channel-asan --target ZEDChannelTests --parallel 4 \
  > /tmp/zed-channel-asan-final-build.log 2>&1
ctest --test-dir ZED/build/channel-asan --output-on-failure \
  > /tmp/zed-channel-asan-test.log 2>&1

lipo -archs ZED/build/channel-release/ZED_artefacts/Release/VST3/ZED.vst3/Contents/MacOS/ZED
codesign --verify --deep --strict --verbose=2 ZED/build/channel-release/ZED_artefacts/Release/VST3/ZED.vst3
/usr/bin/arch -arm64 "$ZED_PLUGINVAL" \
  --strictness-level 5 --random-seed 12345 --output-dir /tmp \
  --output-filename zed-channel-pluginval-tests.txt \
  --validate "$PWD/ZED/build/channel-release/ZED_artefacts/Release/VST3/ZED.vst3" \
  > /tmp/zed-channel-pluginval.log 2>&1
```

Set `ZED_PLUGINVAL` to the executable inside your pluginval 1.0.4 app bundle.
Validation used the official notarized release from the baseline validation. CTest, render
harnesses, and pluginval ran outside the sandbox with approval because JUCE
initializes macOS application services. No tests were disabled.

### Temporary stereo-reference build script

The following is the complete source of
`ZED/build/channel-release/stereo-reference/compare.py` (ignored build output).
It only writes to that ignored directory and never replaces the working source
or production archive. Run it after building the final Release test executable.

```python
from pathlib import Path
import shlex
import shutil
import subprocess

root = Path.cwd()
build = root / 'ZED/build/channel-release'
reference = build / 'stereo-reference'
reference.mkdir(exist_ok=True)
source = reference / 'PluginProcessor.cpp'
source.write_bytes(subprocess.check_output([
    'git', 'show', '4ba0198927765a4feb6604ffe986f60bab0f0953:ZED/Source/PluginProcessor.cpp']))
flags = {}
for line in (build / 'CMakeFiles/ZED.dir/flags.make').read_text().splitlines():
    if ' = ' in line:
        key, value = line.split(' = ', 1)
        flags[key] = shlex.split(value)
subprocess.run(['/usr/bin/c++', *flags['CXX_DEFINES'], *flags['CXX_INCLUDES'],
                *flags['CXX_FLAGS'], '-I' + str(root / 'ZED/Source'),
                '-c', str(source), '-o', str(reference / 'PluginProcessor.cpp.o')], check=True)
archive = reference / 'libZED_Reference.a'
shutil.copy2(build / 'ZED_artefacts/Release/libZED_SharedCode.a', archive)
subprocess.run(['/usr/bin/ar', '-r', str(archive), str(reference / 'PluginProcessor.cpp.o')], check=True)
link = shlex.split((build / 'CMakeFiles/ZEDChannelTests.dir/link.txt').read_text())
link[link.index('-o') + 1] = str(reference / 'ZEDReferenceTests')
link = [str(archive) if part == 'ZED_artefacts/Release/libZED_SharedCode.a' else part for part in link]
subprocess.run(link, cwd=build, check=True)
```

```sh
python3 ZED/build/channel-release/stereo-reference/compare.py > /tmp/zed-stereo-reference-build.log 2>&1
ZED/build/channel-release/ZEDChannelTests_artefacts/Release/ZEDChannelTests \
  --dump-stereo ZED/build/channel-release/stereo-reference/current.bin > /tmp/zed-stereo-current.log 2>&1
ZED/build/channel-release/stereo-reference/ZEDReferenceTests \
  --dump-stereo ZED/build/channel-release/stereo-reference/master.bin > /tmp/zed-stereo-master.log 2>&1
cmp ZED/build/channel-release/stereo-reference/master.bin ZED/build/channel-release/stereo-reference/current.bin
shasum -a 256 ZED/build/channel-release/stereo-reference/{master,current}.bin
```

## Warnings, failures, and limits

- Existing deprecated `Font` constructors: five warnings per full processor build
  at `ZedLookAndFeel.h:139` (four) and `PluginEditor.cpp:141` (one). The temporary
  master processor compilation repeats two `ZedLookAndFeel.h` warnings.
- Existing intermediate VST3 signing diagnostic: `code has no resources but
  signature indicates they must be present`. The existing final ad-hoc signing
  step resolves it; final strict signature verification passes.
- Initial test-target setup used `juce_add_console_app`, causing repeated
  `JUCE_STANDALONE_APPLICATION` macro-redefinition warnings when linked to ZED.
  Replaced with plain `add_executable`; final test builds have no such warnings.
- Initial Release test exited 8 through CTest because the test incorrectly
  required a silent channel to return exact zero. The existing SVF intentionally
  adds `1e-18` in its nonlinear stages. Replaced that assertion with comparison
  to an independently rendered silence reference; DSP was not changed.
- Before integration, `PluginProcessor.cpp` line endings were normalized to LF
  and trailing whitespace removed so ordinary `git diff --check` passes.
  This formatting-only cleanup does not change C++ tokens or processing.
- pluginval skipped the external Steinberg validator because no validator path
  was configured. Its success is not a Steinberg validator result.
- The user confirmed this mono/stereo build passed a manual DAW smoke test on
  an internally stereo Ableton track. Genuine 1-in/1-out host operation remains
  unverified manually. Playing mono material on that track does not establish
  true mono plug-in operation; genuine mono was covered by the automated tests.

The completed bundle is `build/channel-release/ZED_artefacts/Release/VST3/ZED.vst3`
relative to ZED. Logs are under `/tmp/zed-channel-*` and `/tmp/zed-stereo-*`, with
CTest details in each build directory's `Testing/Temporary/LastTest.log`.
Builds, render dumps, the reference script, and logs are not proposed for commit.
