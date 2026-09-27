# ZED 1.0.0 RC1 Windows installer

Supported product targets: native x64 Windows 10 and Windows 11, VST3 only.
ARM64 (including x64 emulation) and 32-bit Windows are rejected by `x64os`.
The installer requires administrator privileges and puts the complete bundle at:

```text
C:\Program Files\Common Files\VST3\ZED.vst3
```

These are Windows known-folder locations, so a non-default system drive is
respected. The uninstaller lives separately in
`Program Files\South Coast Synthesis\ZED`. Destination selection is disabled;
there are no component choices, shortcuts, VST registry registration or launch
actions. Windows Installed Apps / Apps & features provides normal uninstall.
Users are reminded to save work and close DAWs; Restart Manager can detect files
in use, but the installer does not automatically restart applications. With DAWs
closed, no reboot should normally be needed.

## Permanent upgrade identity

**AppId: `{10FF1E2C-BA87-4F89-9628-59B401DB1B3E}`**

Keep this exact ID for future ZED installers. Inno Setup's doubled opening brace
in `ZED.iss` escapes the literal GUID. Future versions update the same installation
and uninstall record. Internal product/installer version is `1.0.0`; the RC label
is for display and distribution filenames, not plug-in version metadata.

Installed files are recorded by Inno Setup and removed on uninstall. There is no
wildcard cleanup that could remove other vendors' plug-ins or user-added files.
Unrecorded files inside ZED's bundle may consequently prevent directory removal;
the CI test checks removal of a clean installation. Future changes to the bundle
layout need their own upgrade review; no broad pre-upgrade deletion is performed.
The source-only BSD licence is not an end-user EULA, so no licence page is shown.

## Build and CI

The existing `.github/workflows/zed-windows.yml` uses `windows-2022`, Visual Studio
2022 x64 and Release. Its CMake targets, CTest tests and raw VST3 upload are retained.
After those succeed, PowerShell invokes the runner's `ISCC.exe` directly. The
script requires exactly one discoverable Inno Setup **6.7.x** compiler (PATH or
the standard Inno Setup 6 Program Files location); it prints the version and fails
if missing, ambiguous or incompatible. Nothing is downloaded or installed to
obtain a compiler. No Inno Setup 7-only directives are used.

Run locally on a suitable Windows machine with PowerShell 7:

```powershell
./scripts/windows/build-installer.ps1 `
  -BundlePath ZED/build/windows-x64-release/ZED_artefacts/Release/VST3/ZED.vst3 `
  -OutputDirectory ZED/build/windows-x64-release/installer
```

Both parameters are required. The output directory must be absent or empty and,
if inside this repository, beneath ignored `ZED/build`. The script validates the
established Release path, unique VST3 binary, PE AMD64/PE32+ DLL headers, non-debug
1.0.0 version resources and VST3 manifest. These checks cannot prove compiler
optimization provenance for an arbitrary renamed binary; the authoritative source
in CI is the immediately preceding `--config Release --target ZED_VST3` build.
Source bundles and output ancestry with reparse points are refused.

Outputs:

- `ZED-1.0.0-rc.1-Windows-x64-Setup.exe`
- `ZED-1.0.0-rc.1-Windows-x64-Setup.exe.sha256` (SHA-256 plus basename, no local path)

Actions artifacts (14-day retention):

- `ZED-1.0.0-Windows-x64-VST3` — unchanged raw bundle fallback, uploaded first.
- `ZED-1.0.0-rc.1-Windows-x64-Installer` — installer and checksum, uploaded only
  after installer mechanics pass. No build tree or logs are uploaded.

Triggers are unchanged: manual `workflow_dispatch`, PRs targeting `master`, and
pushes to `master`. After this feature branch is pushed, choose **Actions → ZED
Windows build → Run workflow → zed-rc1-windows-installer**, or run:

```sh
gh workflow run zed-windows.yml --ref zed-rc1-windows-installer
```

The existing dispatch workflow is already on master, so it can select the feature
branch. No write permissions, signing secrets or publication steps are needed.

## Disposable-runner checks and manual acceptance

`scripts/windows/test-installer.ps1` is guarded for a GitHub-hosted Windows runner
and administrator context. It refuses an existing ZED bundle or uninstall
directory, installs with `/VERYSILENT /SUPPRESSMSGBOXES /NORESTART`, and treats a
restart request as failure. It checks all installed file paths and SHA-256 hashes
against the built bundle, including `Contents\x86_64-win\ZED.vst3`, then invokes
the unique generated uninstaller silently and requires both ZED directories to
be removed. Cleanup is attempted even if installed-file verification fails.
Logs stay under `ZED/build/windows-x64-release/installer-validation`. The test
does not launch a DAW. Windows Server 2022 CI tests installer mechanics; it does
not establish Windows 10/11 DAW compatibility.

On Windows 10 and Windows 11 x64, manually:

1. Verify the downloaded installer checksum; save projects and close DAWs.
2. Run the installer, approve elevation, and check its fixed VST3 destination.
3. Restart/rescan a native x64 VST3 host and load ZED. Check all configurations,
   controls, mono/stereo operation, automation and project save/reopen.
4. Close DAWs, rerun the installer to check in-place replacement and that only
   one Installed Apps entry remains; recheck loading and project recall.
5. Uninstall through Installed Apps and confirm ZED is removed while unrelated
   plug-ins remain intact. Also check the files-in-use prompt with a DAW open.

This installer and uninstaller are **unsigned**. SmartScreen may identify an
unknown publisher or display a reputation warning. No signing claim is made;
Windows code signing can be added separately later. Installers, checksums and
validation logs are generated artifacts and must never be committed.

Local review on macOS is static only: no Windows installer compilation, execution
or DAW test has been performed here. PowerShell/Inno Setup availability and the
complete installer flow must be confirmed by the first GitHub Actions run.
