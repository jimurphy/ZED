# ZED macOS package (Script 1)

From the repository root, run:

```sh
scripts/macos/build-sign-package.sh
```

Requires macOS, Xcode/Apple packaging tools, CMake 3.22+, Git, Python 3.9+,
network access for pinned JUCE and secure timestamps, and valid Developer ID
Application and Installer identities with accessible private keys. macOS may
request Keychain permission. No certificates or keys are exported.

If exactly one valid identity of each type exists it is selected automatically.
Otherwise specify exact certificate names or SHA-1 fingerprints:

```sh
ZED_APPLICATION_IDENTITY='Developer ID Application: <certificate name>' \
ZED_INSTALLER_IDENTITY='Developer ID Installer: <certificate name>' \
ZED_RELEASE_OUTPUT_DIR='/external/release-directory' \
scripts/macos/build-sign-package.sh
```

Identities must belong to the same team. No passwords or credentials are stored.
Output defaults to `../zed-release-artifacts/macos` relative to the repository.
Symlinks are resolved before writing; output inside the worktree is rejected.
An existing `ZED-1.0.0-rc.1-macOS-arm64.pkg` is never overwritten. Choose another
external directory or explicitly move the previous package yourself.

Each invocation uses a fresh, retained `ZED/build/macos-release-package/run.*`
directory with `build`, `payload`, `tmp` and validation data. No recursive cleanup
occurs and no earlier build is altered. Failed runs retain diagnostic material;
an incomplete package may remain in an external `.zed-package.*` directory.
Only a validated package is published under the final filename.

The script configures Release, arm64, macOS 11.0, tests OFF, and builds only
`ZED_VST3` and `ZED_AU`. CMake fetches the existing pinned JUCE release. Original
build products keep their existing local ad-hoc signatures. Staged copies are
signed inside-out with Developer ID Application, hardened runtime and secure
timestamps; no entitlements are added. Existing entitlements cause a stop for
review. Deep verification is used, never deep signing.

The signed flat component installer uses version `1.0.0`, identifier
`com.SouthCoastSynthesis.ZED.pkg` (the existing bundle ID plus `.pkg`), recommended
ownership and install location `/`. Relocation is disabled for both bundles:

- `Library/Audio/Plug-Ins/VST3/ZED.vst3`
- `Library/Audio/Plug-Ins/Components/ZED.component`

Validation checks versions, arm64-only code, strict code signatures, authorities,
team, timestamp, hardened runtime, installer signature, payload paths and bundle
completeness. Expanded payload file hashes must match the signed staging tree.
No install scripts, Distribution XML or installer UI customization are added.

This script does **not** notarize, staple, install, assess Gatekeeper acceptance,
commit or push. The package is not yet ready for public distribution. Host tests
and later notarization are separate steps; this task does not rerun audio tests.

If `codesign` reports `unable to build chain to self-signed root` or
`errSecInternalComponent`, stop and inspect the Developer ID certificate chain
and private-key access in Keychain Access. A listed identity alone does not prove
that distribution signing will succeed. Do not disable trust checks or bypass
Keychain prompts. After correcting the local signing setup, rerun the command
above; the script creates a fresh build/staging run and preserves the failed one.

## Notarization and final validation (Script 2)

`notarize-package.sh` requires macOS Bash, Git, Xcode's `notarytool` and `stapler`,
and the system `pkgutil`, `spctl`, `plutil` and `shasum` tools. It adds no Python,
Homebrew or jq dependency. Network access is needed for submission/stapling and
may be needed by Apple's validation services.

Create a Keychain profile interactively once, following Apple's secure prompts:

```sh
xcrun notarytool store-credentials ZED-notary
```

Do not put credentials in scripts, documentation or shell arguments. Script 2
accepts only a profile name, never a password. The default is `ZED-notary`;
override with `--profile NAME` or `ZED_NOTARY_PROFILE`.

For a new release, first run Script 1, then pass its external package to Script 2:

```sh
scripts/macos/build-sign-package.sh
scripts/macos/notarize-package.sh \
  --package ../zed-release-artifacts/macos/ZED-1.0.0-rc.1-macOS-arm64.pkg
```

Alternatively set `ZED_PACKAGE_PATH`. The path must identify a non-empty regular
`.pkg` outside the repository; package symlinks are refused. Script 2 validates
the Developer ID Installer signature first. If a valid stapled ticket exists,
it skips both submission and stapling. Otherwise normal mode verifies the profile,
submits once with `--wait`, parses Apple's JSON with `plutil`, reports the ID and
status, and staples only after `Accepted`. It never automatically retries a
submission. On failure, inspect the external response/error logs first; if an ID
is available the script attempts to retrieve Apple's notarization log. A network
failure may leave a submission processing at Apple, so do not blindly resubmit.

For the already completed installer, use only:

```sh
scripts/macos/notarize-package.sh --validate-only \
  --package ../zed-release-artifacts/macos/ZED-1.0.0-rc.1-macOS-arm64.pkg
```

This mode never submits, staples or modifies the package. It requires an existing
valid ticket and checks that the package hash stays unchanged. No Keychain profile
lookup is needed when validating an already stapled package.

If Apple accepted a submission but stapling failed, recover without resubmitting:

```sh
scripts/macos/notarize-package.sh --staple-only \
  --package ../zed-release-artifacts/macos/ZED-1.0.0-rc.1-macOS-arm64.pkg
```

All modes require final `stapler validate`, Gatekeeper install assessment with
`source=Notarized Developer ID`, and a trusted Developer ID Installer signature.
Only after all checks pass is `<package>.sha256` created/refreshed beside the
package. It contains the SHA-256 and package basename; verify from that directory
with `shasum -a 256 -c ZED-1.0.0-rc.1-macOS-arm64.pkg.sha256`.
Stapling changes package bytes, so any checksum made before stapling is obsolete.
An older checksum is not refreshed on failure and must not be treated as current.

Logs and temporary working data stay beside the package under
`.notarization/run.*`; existing logs are retained. These may contain submission
IDs and certificate summaries, but the script does not request or log passwords.
Packages, logs and checksums remain outside Git. Neither script installs anything
or commits/pushes changes. Script 2 does not repeat DAW tests.
