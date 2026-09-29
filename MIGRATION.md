# Standalone ZED migration

## Source and extraction

Authoritative source: `https://github.com/jimurphy/audio-projects.git`, fetched
`origin/master` at `de4450e6be8f012a91bca677963462db64e4ac0a`.
The original checkout was clean on master, equal to origin/master, with no local
branches unmerged into origin/master. It remained on that commit, clean and
unchanged after validation. Only the requested source fetch updated remote refs.
The destination `https://github.com/jimurphy/ZED.git` was accessible and returned
no refs. The sibling destination directory did not exist before cloning.

Retained source paths:

- `ZED/` (moved to the root, including its legacy Projucer/IDE files)
- `.github/workflows/zed-windows.yml`
- `scripts/macos/README.md`
- `scripts/macos/build-sign-package.sh`
- `scripts/macos/notarize-package.sh`
- `scripts/windows/build-installer.ps1`
- `scripts/windows/test-installer.ps1`

The monorepo root README and unrelated plug-in directories were not imported.
The source root ignore file contained only the macOS metadata rule, now included
in the standalone ignore file. No additional ZED-specific support files were
found outside the retained paths.

Procedure (from the parent directory, with an absent `ZED` destination):

```sh
git clone --no-local --single-branch --branch master --no-tags \
  https://github.com/jimurphy/audio-projects.git ZED
cd ZED
git rev-parse HEAD
# Required exact result: de4450e6be8f012a91bca677963462db64e4ac0a
git filter-repo --path ZED/ --path .github/workflows/zed-windows.yml \
  --path scripts/macos/ --path scripts/windows/ --path-rename ZED/:
git branch -m main
git remote add origin https://github.com/jimurphy/ZED.git
```

`git-filter-repo` version: `a40bce548d2c`. It removed the source remote automatically.
No reset or rewrite was performed in the original checkout. The clone already
had the exact verified source HEAD, so no additional checkout was needed.

Filtered HEAD: `d8bd13511cdab633f386c48350a811d7486d2c20`, branch `main`,
**101 retained commits**. History is not a snapshot; unrelated-only commits were
pruned. No source tags or other branches were imported. Local filter metadata,
including the old/new commit map, remains under `.git/filter-repo/`.
Historical validation recipes use mapped commit IDs and standalone paths;
short historical identifiers not used by commands remain archival references.

## Uncommitted adaptations

- Root-relative build, workflow, installer and macOS packaging paths.
- Stability measurement output guards now locate this repository's build tree,
  rather than requiring a checkout directory named ZED. This changes only test
  output-location validation, not DSP or test assertions about audio.
- Legacy project include paths replaced with relative paths; no regeneration.
- Historical command examples adapted to build-local output and filtered history.
- Manual-only Windows workflow retained (already present in source master):
  `workflow_dispatch`, no push or pull-request triggers. Jobs, permissions,
  artifacts and installer checks are unchanged apart from paths.
- Root BSD LICENSE, explicit third-party exclusions and README licensing guidance.
- Ignore rules expanded for build, packaging, credentials and per-user outputs.

Production `Source/`, embedded font data in `JuceLibraryCode/`, CMakeLists.txt,
CMakePresets.json, the Inno Setup definition and installer finish text are
byte-identical to the source revision. No DSP, GUI, parameter, state, topology,
identifier, version or JUCE pin change was made. Source-tree Git object IDs were
compared and the corresponding working files have no diff.

## Licensing

Root LICENSE uses standard BSD 3-Clause text, copyright 2020–2026 James Murphy.
Source history begins in 2020, consistent with this range. The grant covers all
original James Murphy-authored ZED code, not only DSP. LICENSE-DSP.md now points
to that broader grant. THIRD_PARTY_NOTICES.md expressly excludes JUCE, SDKs,
fonts, assets and uncertain third-party portions; those files remain unchanged.
The previous narrow audit is retained as historical evidence with a superseded
scope notice. JUCE 8.0.6 remains separately licensed. Font redistribution and
mixed-provenance DSP portions remain unresolved, by explicit user direction;
this migration does not establish permission to distribute those portions.

## History and hygiene audit

552 retained blobs were scanned for high-confidence private-key/token patterns;
none matched. Filename review found no retained certificate, key, installer or
credential files. This is a targeted audit, not a guarantee against every possible
secret format. Current tracked files contain no compiled plug-in/build outputs.
No unrelated plug-in project directories survive extraction. Shared DSP header
comments mentioning older projects are provenance, not imported project trees.

Seven old files remain in history (not in HEAD), intentionally preserved by the
complete-history extraction:

- `Builds/MacOSX/build/Debug/ZED.vst3`
- `Builds/MacOSX/build/Debug/libZED.a`
- `Builds/MacOSX/.DS_Store`
- `Source/.DS_Store`
- Three Xcode per-user files below `Builds/MacOSX/ZED.xcodeproj/`: workspace UI
  state, debugger breakpoints and scheme management.

These include historical local usernames/paths. If history should be free of
old binaries and user state before publication, approve a separate filtered
history cleanup first. Merely ignoring them does not remove historical objects.
No such additional purge was performed. Current legacy include paths were made
portable; old versions remain in history as expected.

No build data is staged or tracked. All migration validation data is under
ignored `build/`. External-package safeguards remain unchanged. No signing
identity, key, password or notarization credential was used or added.

## Validation performed

Tools: CMake 4.4.3, Xcode 26.3 (17C529), Apple Clang 17.0.0
(`clang-1700.6.4.2`), Ruby 2.6.10 YAML, Python 3. Pinned JUCE checkout verified at
`51a8a6d7aeae7326956d747737ccf1575e61e209` (8.0.6).
Commands below start at the standalone repository root:

```sh
mkdir -p build/migration-validation/tmp
TMPDIR="$PWD/build/migration-validation/tmp" cmake -S . -B build/migration-release \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 -DZED_BUILD_TESTS=ON \
  > build/migration-validation/configure.log 2>&1
TMPDIR="$PWD/build/migration-validation/tmp" cmake --build build/migration-release \
  --config Release --target ZED_VST3 ZED_AU ZEDChannelTests ZEDStabilityCharacterisation \
  --parallel 4 > build/migration-validation/build.log 2>&1
TMPDIR="$PWD/build/migration-validation/tmp" ctest --test-dir build/migration-release \
  -C Release --output-on-failure --no-tests=error > build/migration-validation/ctest.log 2>&1
bash -n scripts/macos/build-sign-package.sh
bash -n scripts/macos/notarize-package.sh
git diff --check
ruby -ryaml -e 'd=YAML.load_file(".github/workflows/zed-windows.yml"); abort "Unexpected triggers" unless d[true] == {"workflow_dispatch"=>nil}'
```

Ruby's YAML 1.1 parser treats the `on` key as `true`. A separate parsed comparison
confirmed the workflow differs only by the intended path substitutions. Python
AST parsing of Tests/SummariseStability.py passed. Both Bash checks passed. Git
whitespace checks passed with local `core.whitespace=cr-at-eol`, recognizing the
preserved legacy Windows project CRLF endings without rewriting whole files.

Fresh configuration and all four build targets passed. Both registered CTest
entries passed: ZEDChannelLayouts (6.56 s) and ZEDStabilityToolSmoke (0.29 s),
6.86 s total. The first includes configuration/state, mono/stereo, lifecycle,
sample-rate and topology regressions. Extended stability measurements and earlier
historical comparisons were not repeated. Normal plug-in targets and test opt-in
configuration are unchanged; no sanitizer options were introduced.

Both bundles passed `lipo -archs` and plist inspection using Python plistlib.
The architecture and displayed version were also checked with:

```sh
lipo -archs build/migration-release/ZED_artefacts/Release/VST3/ZED.vst3/Contents/MacOS/ZED
lipo -archs build/migration-release/ZED_artefacts/Release/AU/ZED.component/Contents/MacOS/ZED
/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' build/migration-release/ZED_artefacts/Release/VST3/ZED.vst3/Contents/Info.plist
/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' build/migration-release/ZED_artefacts/Release/AU/ZED.component/Contents/Info.plist
```

| Bundle under `build/migration-release/ZED_artefacts/Release/` | Architecture | Version | Identifier |
| --- | --- | --- | --- |
| `VST3/ZED.vst3` | arm64 only | 1.0.0 | com.SouthCoastSynthesis.ZED |
| `AU/ZED.component` | arm64 only | 1.0.0 | com.SouthCoastSynthesis.ZED |

Executable in each: `Contents/MacOS/ZED`. AU remains `aufx / Zedd / Soco` with
factory `ZEDAUFactory`; deployment target remains 11.0. Version checks covered
both CFBundleShortVersionString and CFBundleVersion. No plug-in auto-copy/install
occurs (`COPY_PLUGIN_AFTER_BUILD FALSE`). Existing local ad-hoc sealing ran as
part of the unchanged normal build. No Developer ID signing, installer signing,
packaging, notarization, stapling, installation or publication occurred.

Warnings: existing deprecated JUCE Font constructor warnings (five diagnostics
across two source files); existing local ad-hoc signature replacement notices.
No build or test failure occurred. PowerShell is not installed, so its two scripts
received static path/diff review only, not parser or Windows execution validation.
No Windows workflow, pluginval, auval, sanitizer or manual DAW test was run during
this migration. Packaging/signing/notarization scripts were never executed.

Searches found no obsolete monorepo paths in operational files. Source URL/path
names in this migration report describe provenance intentionally. No new manual
was invented. No commit, push, tag, release, pull request, Actions run, remote
settings or visibility change was performed.

## Review handoff

The extracted repository is ready for local review of migration changes, not a
claim of public distribution clearance. Review the unresolved third-party/font
licensing and decide whether historical binaries/user state should be purged
before a later commit-and-push task. This task leaves all edits uncommitted and
origin pointing only to the empty ZED destination.
