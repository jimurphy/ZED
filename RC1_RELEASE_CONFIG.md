# ZED 1.0.0 release configuration

> Historical validation record. Commands, output paths and applicable commit IDs
> have been adapted to the standalone repository; the recorded results were not
> rerun during migration unless listed in MIGRATION.md.

Prepared on `zed-rc1-release-config` from clean, fast-forward-current master
`c688c33` for an eventual `1.0.0-rc.1`. No RC tag or public release is created.
The project and host-facing version is **1.0.0**, without an RC suffix.

## Scope and metadata

No production source, DSP, parameter, state, GUI or test implementation changed.
The complete ZED source/resource/build-metadata audit found no earlier product
version to replace. Legacy generated product versions already say 1.0.0;
`ZED.jucer` uses its default rather than an explicit version attribute. CMake's
`project(VERSION 1.0.0)` is canonical for this build and is explicitly passed to
`juce_add_plugin`. Legacy Projucer/IDE files were neither edited nor regenerated.
The GUI displays no release-version label, so none was added. Neither
the former project-level `docs/USER_GUIDE.md` nor the repository-root `docs/USER_GUIDE.md` existed;
no manual was created or modified.

| Field | Preserved/final value |
| --- | --- |
| Product / description | `ZED` / `ZED` |
| Project, VST3 and AU version | `1.0.0`; AU integer `65536` (`0x10000`) |
| Manufacturer | `South Coast Synthesis` |
| Manufacturer code | `Soco` (`0x536f636f`) |
| Plug-in code / AU subtype | `Zedd` (`0x5a656464`) |
| VST3 processor CID | `ABCDEF019182FAEB536F636F5A656464` |
| VST3 controller CID | `ABCDEF011234ABCD536F636F5A656464` |
| VST3 compatibility CID | `ABCDEF01C0DEF00D536F636F5A656464` |
| VST3 category | `Fx|Filter`; VST2 replacement remains disabled |
| AU type / subtype / manufacturer | `aufx / Zedd / Soco` |
| AU factory | `ZEDAUFactory`, prefix `ZEDAU` |
| AU display name | `South Coast Synthesis: ZED` |
| Bundle identifier, both formats | `com.SouthCoastSynthesis.ZED` |
| Product copyright | `southcoastsynthesis` |
| Manufacturer website / email | Empty, unchanged; no support address invented |
| Parameters | `drive`, `cutoff`, `resonance`, `filterConfiguration`, unchanged |
| APVTS state identifier | `Zed`, unchanged |
| JUCE | 8.0.6, pinned commit `51a8a6d7aeae7326956d747737ccf1575e61e209` |

No required identifier was missing or invalid. AU codes and factory match the
retained legacy AU plist. The final VST3 `moduleinfo.json` is byte-identical to
the preceding topology build's manifest, including all three class identities.

## Formats, deployment and JUCE Starter

CMake requests **VST3 + AU on Apple platforms, VST3 only on Windows**. No AAX,
standalone, AUv3, VST2, Intel or Universal Binary product was added. JUCE's
wrapper build lists source files for other formats, but their wrappers are
disabled by generated format macros; those filenames do not imply extra products.

macOS defaults are ARM64 and **11.0**, set before compiler detection. The 11.0
minimum is retained from the established preset, not newly selected. Both final
Mach-O executables report `minos 11.0`, SDK 26.2, and only the arm64 slice.
Apple defaults and sealing commands are conditional; no Apple architecture or
link option is applied to a Windows/MSVC configuration. A Windows x64 build
was not attempted and remains unverified on Windows.

The intended JUCE licence is JUCE 8 Starter. The pinned JUCE 8.0.6
`modules/juce_gui_basics/juce_gui_basics.cpp` explicitly warns that
`JUCE_DISPLAY_SPLASH_SCREEN` is ignored: the historical splash implementation
is removed. Therefore CMake does not define the obsolete flag. Old generated
IDE projects already define it as zero but are not used here. This configuration
does not change the JUCE pin or grant a JUCE licence.

Local ad-hoc signing was explicitly permitted for host testing after clarifying
the initial no-signing instruction. Both completed bundles are sealed with
`codesign --force --sign - --timestamp=none`. There is no Developer ID,
certificate, team identity, secure timestamp, notarisation or installer.
`COPY_PLUGIN_AFTER_BUILD` remains false. The AU was manually copied by the
validation workflow into the normal user-local location described below;
the build itself installed nothing.

## DSP licence boundary (historical audit)

**Superseded scope:** the standalone migration extends BSD 3-Clause to all
original James Murphy-authored ZED code. The root LICENSE and
THIRD_PARTY_NOTICES.md now define the grant and exclusions. The narrow grant
below records the earlier decision, not the current licensing scope. Its
third-party provenance findings remain unresolved.

[LICENSE-DSP.md](LICENSE-DSP.md) contains the standard BSD 3-Clause text with
`Copyright (c) 2026 Jim Murphy`. Its exact scope is only:

- `Source/jdsplib/ZDSKmm.h`: Jim Murphy's wrapper, routing and lifecycle forwarding.
- `Source/jdsplib/gain.h`: Jim Murphy's gain setter/multiplier utility (not an active filter engine).

Their included dependencies are **not** transitively relicensed. This is a
narrow grant, not a claim that all ZED DSP or the whole plug-in is BSD-licensed.

The following audit findings require provenance clarification before expanding
the grant:

| Files under `Source/jdsplib/` | Reason excluded |
| --- | --- |
| `ZDSVF.h`, `ZDSK.h`, `ZDSKHPF.h`, `ZDML.h`, `ZDDL.h`, `ZDOnePole.h`, `ZDOnePoleEx.h` | Jim Murphy author credit alongside Pirkle/Zavalishin source/app-note references; independent implementation versus adapted code is not established |
| `DSPMath.h` | Explicit attributions to Martijn Zwartjes, Aleksey Vaneev, an external gist and Arduino |
| `OnePoleLP.h`, `DCBlocker.h` | References to MusicDSP / Stanford software implementations |
| `DelayLin.h`, `DiffusionDelayLin.h`, `RevMZ.h` | Explicitly based on Martijn Zwartjes code |
| `Adsr.h`, `lfo.h`, `oscillatormm.h` | Martijn/Pirkle implementation references |
| `CombFilterFB.h`, `CombFilterFBLP.h`, `CombFilterFF.h` | Stanford/Freeverb references; not part of the narrowed active-filter grant |
| `RevMini.h`, `voice.h` | Bundled, unused in ZED's active path; not reviewed sufficiently for a broader grant |

An algorithm citation alone does not establish copied code, but neither does an
author header settle the provenance of every expression. No blanket ownership
assumption was made. The user can clarify independent authorship/permissions in
a later licensing review. Processor scaffolding, JUCE, fonts (including
`Galvji.ttc`), assets, GUI artwork and unrelated infrastructure are excluded.
Existing attribution comments are preserved. No source file was rewritten to
change its licensing boundary.

## Local build and validation

New working directories are `build/rc1-release` and
`build/rc1-validation`. Earlier build directories were not deleted or rebuilt;
the pinned JUCE source and existing pluginval application were reused read-only.
Everything generated remains ignored. Tools: CMake 4.4.3; Apple Clang 17.0.0
(`clang-1700.6.4.2`); Xcode 26.3 (17C529); macOS 15.7.3 (24G419), native ARM64;
pluginval 1.0.4; Apple auval 1.10.0.

Commands, from the repository root:

```sh
mkdir -p build/rc1-validation/tmp
export TMPDIR="$PWD/build/rc1-validation/tmp"
cmake -S . -B build/rc1-release \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 -DZED_BUILD_TESTS=ON \
  -DFETCHCONTENT_SOURCE_DIR_JUCE="$PWD/build/macos-arm64-release/_deps/juce-src" \
  > build/rc1-validation/configure.log 2>&1
cmake --build build/rc1-release \
  --target ZEDChannelTests ZEDStabilityCharacterisation ZED_VST3 ZED_AU --parallel 4 \
  > build/rc1-validation/build.log 2>&1
ctest --test-dir build/rc1-release --output-on-failure \
  > build/rc1-validation/ctest.log 2>&1

# After adding the AU bundle-sealing step (no production source correction):
cmake --build build/rc1-release --target ZED_VST3 ZED_AU --parallel 4 \
  > build/rc1-validation/bundle-seal-build.log 2>&1

for bundle in build/rc1-release/ZED_artefacts/Release/VST3/ZED.vst3 \
              build/rc1-release/ZED_artefacts/Release/AU/ZED.component; do
  lipo -archs "$bundle/Contents/MacOS/ZED"
  xcrun vtool -show-build "$bundle/Contents/MacOS/ZED"
  plutil -lint "$bundle/Contents/Info.plist"
  codesign --verify --deep --strict --verbose=2 "$bundle"
done
nm -gU build/rc1-release/ZED_artefacts/Release/AU/ZED.component/Contents/MacOS/ZED \
  | rg ZEDAUFactory
cmp build/topology-switch-release/ZED_artefacts/Release/VST3/ZED.vst3/Contents/Resources/moduleinfo.json \
    build/rc1-release/ZED_artefacts/Release/VST3/ZED.vst3/Contents/Resources/moduleinfo.json

arch -arm64 build/topology-switch-validation/tools/pluginval.app/Contents/MacOS/pluginval \
  --strictness-level 5 --random-seed 12345 \
  --output-dir "$PWD/build/rc1-validation" \
  --output-filename pluginval-final-tests.txt \
  --validate "$PWD/build/rc1-release/ZED_artefacts/Release/VST3/ZED.vst3" \
  > build/rc1-validation/pluginval-final-console.log 2>&1
```

The AU installation was guarded against overwriting a file or symlink:

```sh
if [ -e "$HOME/Library/Audio/Plug-Ins/Components/ZED.component" ] || \
   [ -L "$HOME/Library/Audio/Plug-Ins/Components/ZED.component" ]; then
  echo "Refusing to overwrite an existing ZED.component"
  exit 1
fi
mkdir -p "$HOME/Library/Audio/Plug-Ins/Components"
ditto build/rc1-release/ZED_artefacts/Release/AU/ZED.component \
  "$HOME/Library/Audio/Plug-Ins/Components/ZED.component"
arch -arm64 /usr/bin/auval -v aufx Zedd Soco > build/rc1-validation/auval.log 2>&1
# Initial registration was not yet visible. After discovery, retry succeeded:
arch -arm64 /usr/bin/auval -al > build/rc1-validation/auval-discovery.log 2>&1
arch -arm64 /usr/bin/auval -v aufx Zedd Soco > build/rc1-validation/auval-retry.log 2>&1
```

The first copy has already been installed; do not repeat the guarded copy as
though the destination were still empty. Discovery confirmed this exact
user-local component, not another AU. No system-wide directory or existing
plug-in was modified. No AU cache was deleted and no audio service was killed.

| Check | Result |
| --- | --- |
| Release targets | Both test executables, VST3 and AU built successfully |
| CTest | 2/2 passed in 6.76 seconds: existing layout/parameter/state/editor/lifecycle/topology suites plus characterisation-tool smoke test |
| Architecture/deployment | Both arm64-only, macOS 11.0 minimum |
| Bundle structure | Executables, valid Info.plists and PkgInfo files present; VST3 resource manifest present; AU factory exported and component description correct |
| Final bundle signatures | Both strict verification passed; ad-hoc only, no team identity or timestamp |
| VST3 identity | Entire final manifest matches preceding baseline byte for byte |
| pluginval | Final bundle: strictness 5, seed 12345, SUCCESS, exit 0 |
| Apple auval | Retry: AU VALIDATION SUCCEEDED, exit 0; version 1.0.0, mono rendering and parameter scheduling pass |
| Steinberg standalone validator | Unavailable in the existing checked tools; pluginval reports it skipped, not passed |

Warnings and limitations:

- Existing JUCE `Font` deprecation warnings in `ZedLookAndFeel.h` and
  `PluginEditor.cpp` match the baseline; no unrelated GUI edits were made.
- JUCE's intermediate VST3 "code has no resources" message is the existing
  signing sequence; the completed bundle verifies correctly. The newly built
  AU initially had the same verification problem with its linker-only signature.
  Extending the final ad-hoc seal to AU corrected packaging without DSP changes.
- Initial auval exit 2 said it could not find the component (version retrieval
  error -50). Discovery/retry resolved registration without changing the binary.
- `auval` exercises some additional sample rates, but its pass is not evidence
  that all settings at those rates are safe. The supported rates remain
  44.1/48/88.2/96/192 kHz; the known out-of-spec 32 kHz finding in
  `STABILITY_CHARACTERISATION.md` is unchanged.
- Extended characterisation, sanitizer runs and listening tests were not
  repeated for this configuration-only change. Windows and actual macOS 11.0
  runtime compatibility were not tested on separate machines.
- The narrowed BSD scope and ambiguous DSP provenance remain an explicit
  licensing-review item before any broader source-licence claim.

## Artifacts and manual handoff

Built bundles (repository-relative):

- `build/rc1-release/ZED_artefacts/Release/VST3/ZED.vst3`
- `build/rc1-release/ZED_artefacts/Release/AU/ZED.component`

Each executable is `Contents/MacOS/ZED` inside its bundle. The AU test copy
remains at `~/Library/Audio/Plug-Ins/Components/ZED.component`. No VST3 was
installed or replaced. Use the built VST3's parent directory as Ableton Live's
custom VST3 folder, or manually manage a test copy in the user-local VST3 folder
without overwriting an existing plug-in unintentionally.

In native Ableton Live, rescan and load the VST3. In Logic Pro or GarageBand,
rescan/restart as needed and load `South Coast Synthesis: ZED` from the installed
AU. Test all eight configurations, cutoff/resonance/drive automation, mono and
stereo layouts, editor reopen, and save/close/reopen of a project. Compare the
ordinary sound and controls with the existing build, at 44.1/48/96 kHz at least.
Minor immediate switching clicks remain accepted. Internally stereo tracks
with mono source material do not prove a genuine 1-in/1-out plug-in layout.
No manual host listening or session-recall result is claimed here.

All changes are left uncommitted for inspection. No tag, push, GitHub release,
installer or notarisation was created; only the explicitly permitted local
ad-hoc seals and user-local AU test installation were performed.
