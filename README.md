# ZED CMake build

Filter selection now uses the single eight-choice `filterConfiguration`
parameter. See [the parameter contract and validation](FILTER_CONFIGURATION.md)
for its fixed ordering, GUI rules and intentional break from legacy selectors.

See [baseline validation results](VALIDATION.md) for the fresh arm64 build,
exact tool versions, portable build commands, pluginval results, and Git exclusions.

The CMake build is independent of `ZED.jucer`, `JuceLibraryCode`, and the existing
projects under `Builds`. It builds VST3 and AU on macOS, VST3 only on Windows,
and embeds `Source/Galvji.ttc`. The canonical project/host version is `1.0.0`.
See [the RC1 configuration audit](RC1_RELEASE_CONFIG.md) for the local build,
metadata, validation and licensing boundary. No release tag is implied.

Requires CMake 3.22 or newer, Git, and Xcode with its macOS SDK. The first configure
downloads [JUCE 8.0.6](https://github.com/juce-framework/JUCE/releases/tag/8.0.6),
pinned to release commit `51a8a6d7aeae7326956d747737ccf1575e61e209`.
No external JUCE installation or Projucer run is needed.

From this directory:

```sh
cmake --preset macos-arm64-release
cmake --build --preset macos-arm64-release --parallel 4
```

The preset selects Release, C++17, arm64, and a macOS 11.0 deployment target.
All generated headers, binary resources, dependencies, and build products go under
the ignored `build/` directory. The completed bundles are:

```text
build/macos-arm64-release/ZED_artefacts/Release/VST3/ZED.vst3
build/macos-arm64-release/ZED_artefacts/Release/AU/ZED.component
```

Verify its executable:

```sh
lipo -archs build/macos-arm64-release/ZED_artefacts/Release/VST3/ZED.vst3/Contents/MacOS/ZED
lipo -archs build/macos-arm64-release/ZED_artefacts/Release/AU/ZED.component/Contents/MacOS/ZED
```

For manual testing in native Ableton Live 12, use the bundle's containing `VST3`
directory as Live's VST3 custom folder, or copy the entire bundle to
`~/Library/Audio/Plug-Ins/VST3/` and enable VST3 system folders in Live. Rescan,
then load ZED on a stereo audio track. The build does not install the plug-in
automatically. Host loading, UI, audio, automation, and session recall need manual
testing before accepting the migration.
See [Ableton's macOS plug-in setup instructions](https://help.ableton.com/hc/en-us/articles/209068929-Using-AU-and-VST-plug-ins-on-macOS).

## Preserved project settings

- Name/description: `ZED`; version: `1.0.0`.
- Manufacturer: `South Coast Synthesis`; manufacturer code: `Soco`.
- Plug-in code: `Zedd`; bundle identifier: `com.SouthCoastSynthesis.ZED`.
- VST3 category: `Fx|Filter`; VST2 replacement disabled.
- AU type/subtype/manufacturer: `aufx / Zedd / Soco`; factory prefix: `ZEDAU`.
- Audio effect with no MIDI input/output and no editor keyboard focus requirement.
- Continuous parameter IDs, ranges and defaults are retained. The subsequent
  channel-layout and filter-configuration changes are documented in
  [CHANNEL_VALIDATION.md](CHANNEL_VALIDATION.md) and
  [FILTER_CONFIGURATION.md](FILTER_CONFIGURATION.md).

The CMake build disables unused browser and curl support and retains the existing
strict reference-counted pointer setting.

## Source compatibility

No source compatibility edits were required for JUCE 8.0.6 at C++17. The existing
`Font` constructors emit deprecation warnings but still compile; they are retained
to keep this migration limited to build changes. CMake generates its own
`JuceHeader.h` and `BinaryData.h`, so the checked-in Projucer outputs are neither
used nor regenerated.

JUCE 8.0.6 generates the VST3 `moduleinfo.json` after its initial macOS signing
step. A final CMake post-build command ad-hoc seals both completed bundles so that
the VST3 manifest and AU bundle resources are covered. This is for local testing
only: no Developer ID, signing certificate, timestamp or notarization is used.

JUCE 8.0.6 no longer implements the historical "Made with JUCE" splash screen.
The obsolete `JUCE_DISPLAY_SPLASH_SCREEN` flag is therefore not defined in this
CMake build; this release emits an ignored-flag warning if it is defined.
JUCE remains under its own licence (the intended release uses JUCE 8 Starter).
[LICENSE-DSP.md](LICENSE-DSP.md) grants BSD 3-Clause only for its explicitly
listed original files; it does not license JUCE, assets or the entire plug-in.
