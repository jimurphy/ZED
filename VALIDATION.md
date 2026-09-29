# CMake baseline validation — 2026-09-17

> Historical validation record. Commands, output paths and applicable commit IDs
> have been adapted to the standalone repository; the recorded results were not
> rerun during migration unless listed in MIGRATION.md.

Branch: `zed-cmake-migration`. Starting HEAD:
`63eb230bf671e247f348038e50b162e5e17165d3`.
During validation, no source edits or Projucer/IDE regeneration were performed.
The user reported successful Ableton Live 12 testing of the preceding CMake build.
This fresh build was tested with pluginval, not separately in Live.

## Dependency and build environment

JUCE is pinned in `CMakeLists.txt` to
`51a8a6d7aeae7326956d747737ccf1575e61e209`, the exact commit for tag
[`8.0.6`](https://github.com/juce-framework/JUCE/releases/tag/8.0.6).
The dependency checkout was clean; both `HEAD` and `refs/tags/8.0.6` resolved to
that SHA. The fresh build reused only that verified source checkout, not its
previous objects, cache, generated headers, or helper executables.

| Tool | Exact version |
| --- | --- |
| macOS | 15.7.3, build 24G419, arm64 |
| CMake | 4.4.3 (Homebrew) |
| Xcode | 26.3, build 17C529 |
| macOS SDK | 26.2 |
| Apple Clang | 17.0.0, clang-1700.6.4.2 |
| Linker | ld-1230.1 |
| Make | GNU Make 3.81 (`/usr/bin/make`) |
| Git | 2.50.1 (Apple Git-155) |
| pluginval | 1.0.4, bundled JUCE 8.0.3, official universal macOS binary run as arm64 |

Apple's signing and architecture tools did not accept version flags. Exact binary
SHA-256 fingerprints:

- `/usr/bin/codesign`: `5c1748ba916ce6c46190c69009653e2455676953582edd79fda2a2d8f7470d61`
- Xcode toolchain `usr/bin/lipo`: `8cbcbb200dcfa62bc5f709f9cd374b8a5582741217c3d41ffa52d2849e8f24ad`

## Fresh build commands

Run from the repository root. The commands below retain the tested options,
with local paths replaced by portable variables for publication. `ZED_VALIDATION_DIR`
represents the external temporary directory used for validation. The destination
did not contain a previous build. Configuration and compilation both exited 0.
The source override assumes the documented preset has already fetched JUCE.
For a new checkout, omit `FETCHCONTENT_SOURCE_DIR_JUCE` to let CMake fetch the pin.

```sh
mkdir -p build
ZED_VALIDATION_DIR="$(mktemp -d "$PWD/build/zed-baseline.XXXXXXXX")"
ZED_JUCE_SOURCE="$(pwd)/build/macos-arm64-release/_deps/juce-src"
cmake -S . -B "$ZED_VALIDATION_DIR/release" \
  -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DFETCHCONTENT_SOURCE_DIR_JUCE="$ZED_JUCE_SOURCE" \
  > "$ZED_VALIDATION_DIR/configure.log" 2>&1
cmake --build "$ZED_VALIDATION_DIR/release" \
  --config Release --target ZED_VST3 --parallel 4 \
  > "$ZED_VALIDATION_DIR/build.log" 2>&1
lipo -archs "$ZED_VALIDATION_DIR/release/ZED_artefacts/Release/VST3/ZED.vst3/Contents/MacOS/ZED"
codesign --verify --deep --strict --verbose=2 "$ZED_VALIDATION_DIR/release/ZED_artefacts/Release/VST3/ZED.vst3"
```

C++17 is required by `CMakeLists.txt`. Architecture result: `arm64`.
Final signature: `valid on disk`, `satisfies its Designated Requirement`.

Completed bundle:

```text
$ZED_VALIDATION_DIR/release/ZED_artefacts/Release/VST3/ZED.vst3
```

## pluginval

Downloaded the [official v1.0.4 release](https://github.com/Tracktion/pluginval/releases/tag/v1.0.4)
to temporary storage; no system installation, quarantine removal, or re-signing
of pluginval was necessary. Archive SHA-256:
`3c4c533bda0c5059eea3ddaea752d757ee2025041f0f47e6bcb0e87f6082b29f`.

```sh
curl -fL https://api.github.com/repos/Tracktion/pluginval/releases/tags/v1.0.4 \
  -o "$ZED_VALIDATION_DIR/pluginval-release.json"
curl -fL https://github.com/Tracktion/pluginval/releases/download/v1.0.4/pluginval_macOS.zip \
  -o "$ZED_VALIDATION_DIR/pluginval_macOS.zip"
unzip -q "$ZED_VALIDATION_DIR/pluginval_macOS.zip" \
  -d "$ZED_VALIDATION_DIR/tools"
codesign --verify --deep --strict --verbose=2 "$ZED_VALIDATION_DIR/tools/pluginval.app"
spctl --assess --type execute --verbose=2 "$ZED_VALIDATION_DIR/tools/pluginval.app"
/usr/bin/arch -arm64 "$ZED_VALIDATION_DIR/tools/pluginval.app/Contents/MacOS/pluginval" \
  --strictness-level 5 --random-seed 12345 \
  --output-dir "$ZED_VALIDATION_DIR" \
  --output-filename pluginval-tests.txt \
  --validate "$ZED_VALIDATION_DIR/release/ZED_artefacts/Release/VST3/ZED.vst3" \
  > "$ZED_VALIDATION_DIR/pluginval-console.log" 2>&1
```

Network access, application trust verification, and pluginval execution required
approved execution outside the sandbox. Trust verification passed and Gatekeeper
reported `accepted`, `source=Notarized Developer ID`.

Result: **SUCCESS, exit 0**, strictness 5, seed `0x3039`, with no disabled tests
or skipped GUI tests. Tested sample rates: 44100, 48000, 96000 Hz; block sizes:
64, 128, 256, 512, 1024. Editor, editor while processing, state, automation,
editor automation, and bus tests completed. This is a baseline validation result,
not evidence that the deferred realtime-state or mono-processing issues are fixed.

Steinberg VST3 validator: **not available in the checked locations; not run**.
Checked PATH, Applications, Homebrew, Documents, Downloads, and `/Library/Audio`.
pluginval explicitly reported:
`INFO: Skipping vst3 validator as validator path hasn't been set`.
Its `auval` test heading also appears, but the artifact under test is VST3;
this does not establish AU validation.

## All warnings and failed probes

- Five compiler warnings, all `-Wdeprecated-declarations` for existing `Font`
  constructors: four at `Source/ZedLookAndFeel.h:139`, one at
  `Source/PluginEditor.cpp:141`. Diagnostic: use the constructor taking
  `FontOptions`. No source changes were made.
- Intermediate build signing reports `code has no resources but signature
  indicates they must be present`, followed by replacement with an ad-hoc
  signature. JUCE then generates `moduleinfo.json`; the existing final signing
  command seals the complete bundle. Final strict verification passes.
- Sandboxed Xcode SDK version queries emitted `DVTFilePathFSEvents: Failed to
  start fs event stream` and `DVTDeveloperPaths: Failed to get length of
  DARWIN_USER_CACHE_DIR from confstr(3)` with POSIX error 5, falling back to
  `NSCachesDirectory`. SDK version was still returned.
- Initial sandboxed GitHub metadata download failed with curl exit 6,
  `Could not resolve host: api.github.com`. Approved network retry succeeded.
- Initial sandboxed pluginval signature check reported an invalid arm64
  signature; its sandboxed `--help` launch aborted with exit 134 and no output.
  Outside the sandbox, help exited 0 and signature/notarization checks passed
  on the same unmodified download. Actual validation then exited 0.
- Validator discovery could not inspect the installed `Privileges.app`
  (`Permission denied`); the optional user SDK directory does not exist. No validator
  was found in the accessible searched locations.
- `codesign --version` and `lipo -version` are unsupported and failed. Tool
  fingerprints are recorded above instead.
- Steinberg validation was skipped as described above. No pluginval test
  failures or additional warnings appeared in the completed validation log.

Raw configure, build, pluginval console/test logs and the downloaded tool remain
under `$ZED_VALIDATION_DIR/`; temporary files may be cleaned by
macOS. They are outside Git.

## Git exclusions and preserved files

The repository `.gitignore` excludes macOS `.DS_Store` (including the lowercase
`.DS_store` spelling). ZED's `.gitignore` excludes its actual CMake build tree,
legacy macOS build products, local CMake presets/cache/generated files, and
Xcode per-user state. The legacy shared Xcode and Visual Studio project files
remain eligible for tracking; `Builds/` is not broadly ignored.

The fresh build, downloaded validator, and raw logs remain outside the repository.
Source, DSP, parameters, identifiers, GUI, and shared legacy projects are unchanged.

For the baseline commit, seven legacy artifacts are removed from Git tracking
while their local copies are retained: two Finder metadata files, three Xcode
per-user state files, the compiled `libZED.a`, and the old `ZED.vst3` symlink
(which pointed to a machine-specific installation path). Shared project files
and embedded source resources remain tracked.
