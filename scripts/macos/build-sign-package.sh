#!/bin/bash
# Bash 3.2 compatible. Build/package only; never install or notarize.
set -euo pipefail
umask 022
trap 'printf "ERROR: line %s failed; no installation was performed.\n" "$LINENO" >&2' ERR
die() { printf 'ERROR: %s\n' "$*" >&2; exit 1; }
progress() { printf '\n==> %s\n' "$*"; }
if [ "${1:-}" = --help ]; then
    cat <<'HELP'
Usage: scripts/macos/build-sign-package.sh
Optional environment variables:
  ZED_APPLICATION_IDENTITY   Exact Developer ID Application name or SHA-1
  ZED_INSTALLER_IDENTITY     Exact Developer ID Installer name or SHA-1
  ZED_RELEASE_OUTPUT_DIR     External destination directory
Missing identities are selected only when exactly one valid match exists.
Existing installer files are never overwritten. No force option is provided.
HELP
    exit 0
fi
[ "$#" -eq 0 ] || die 'Unknown arguments; use --help.'
[ "$(uname -s)" = Darwin ] || die 'This script requires macOS.'
for tool in git cmake python3 security codesign pkgbuild pkgutil ditto lipo; do
    command -v "$tool" >/dev/null || die "Required tool missing: $tool"
done
script_dir=$(cd "$(dirname "$0")" && pwd -P)
repo=$(git -C "$script_dir" rev-parse --show-toplevel)
repo=$(cd "$repo" && pwd -P)
bundle_id=com.SouthCoastSynthesis.ZED
package_id=$bundle_id.pkg
filename=ZED-1.0.0-rc.1-macOS-arm64.pkg

# Resolve symlinks and .. before creating anything, then check again afterwards.
output=$(python3 - "$repo" "${ZED_RELEASE_OUTPUT_DIR:-$(dirname "$repo")/zed-release-artifacts/macos}" <<'PY'
import os, sys
repo, path = map(os.path.realpath, sys.argv[1:])
if os.path.commonpath([repo, path]) == repo:
    sys.exit('Release output must be outside the Git worktree (including symlinks).')
if path in ('/', os.path.expanduser('~')):
    sys.exit('Choose a dedicated release-output directory, not root or home.')
print(path)
PY
)
[ ! -e "$output/$filename" ] && [ ! -L "$output/$filename" ] || die "Installer already exists: $output/$filename"

select_identity() {
    local kind=$1 policy=$2 requested=$3 variable=$4 identities
    identities=$(security find-identity -v -p "$policy")
    printf '%s\n' "$identities" | python3 -c '
import re, sys
kind, requested, variable = sys.argv[1:]
matches = set()
for line in sys.stdin:
    m = re.match(r"\s*\d+\) ([A-Fa-f0-9]{40}) \"([^\"]+)\"\s*$", line)
    if m and m[2].startswith(kind + ": "):
        if not requested or requested == m[2] or requested.upper() == m[1].upper():
            matches.add((m[1], m[2]))
if len(matches) != 1:
    sys.exit("Expected exactly one valid matching " + kind + " identity; set " + variable + " to its exact name or SHA-1 (found " + str(len(matches)) + ").")
print(next(iter(matches))[1])
' "$kind" "$requested" "$variable"
}
app_identity=$(select_identity 'Developer ID Application' codesigning "${ZED_APPLICATION_IDENTITY:-}" ZED_APPLICATION_IDENTITY)
installer_identity=$(select_identity 'Developer ID Installer' basic "${ZED_INSTALLER_IDENTITY:-}" ZED_INSTALLER_IDENTITY)
app_team=${app_identity##*(}; app_team=${app_team%)}
installer_team=${installer_identity##*(}; installer_team=${installer_team%)}
[ "$app_team" = "$installer_team" ] || die 'Application and Installer identities belong to different teams.'
progress "Application identity: $app_identity"
progress "Installer identity: $installer_identity"

# Refuse symlinked build roots. Each invocation has a unique staging tree, so
# cleanup requires no recursive deletion and previous runs remain reviewable.
[ ! -L "$repo/build" ] || die 'build must not be a symlink.'
mkdir -p "$repo/build"
build_area=$(cd "$repo/build" && pwd -P)
[ "$build_area" = "$repo/build" ] || die 'Build area resolves outside the expected project path.'
work=$build_area/macos-release-package
[ ! -L "$work" ] || die 'Packaging work directory must not be a symlink.'
mkdir -p "$work"
run=$(mktemp -d "$work/run.XXXXXXXX")
build=$run/build
stage=$run/payload
mkdir -p "$stage" "$run/tmp"
export TMPDIR=$run/tmp
progress "Build: $build; staging: $stage"
cmake -S "$repo" -B "$build" -G 'Unix Makefiles' \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 -DZED_BUILD_TESTS=OFF
cmake --build "$build" --config Release --target ZED_VST3 ZED_AU --parallel 4

check_bundle() {
    local bundle=$1 executable arch
    [ -d "$bundle" ] || die "Missing expected bundle: $bundle"
    [ "$(/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "$bundle/Contents/Info.plist")" = "$bundle_id" ] || die 'Unexpected bundle identifier.'
    [ "$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' "$bundle/Contents/Info.plist")" = 1.0.0 ] || die 'Unexpected release version.'
    [ "$(/usr/libexec/PlistBuddy -c 'Print :CFBundleVersion' "$bundle/Contents/Info.plist")" = 1.0.0 ] || die 'Unexpected build version.'
    executable=$(/usr/libexec/PlistBuddy -c 'Print :CFBundleExecutable' "$bundle/Contents/Info.plist")
    [ "$executable" = ZED ] || die 'Unexpected executable name.'
    arch=$(lipo -archs "$bundle/Contents/MacOS/$executable")
    [ "$arch" = arm64 ] || die "Expected arm64 only, found: $arch"
    printf '%s: version 1.0.0, architecture %s\n' "$bundle" "$arch"
}
products=$build/ZED_artefacts/Release
check_bundle "$products/VST3/ZED.vst3"
check_bundle "$products/AU/ZED.component"
mkdir -p "$stage/Library/Audio/Plug-Ins/VST3" "$stage/Library/Audio/Plug-Ins/Components"
ditto "$products/VST3/ZED.vst3" "$stage/Library/Audio/Plug-Ins/VST3/ZED.vst3"
ditto "$products/AU/ZED.component" "$stage/Library/Audio/Plug-Ins/Components/ZED.component"

# Inspect and sign individual Mach-O files and nested bundles deepest first.
# No --deep signing and no entitlement assumptions. Reject escaping symlinks.
progress 'Inspecting and signing staged code (Keychain may request permission)'
python3 - "$stage" "$app_identity" "$app_team" <<'PY'
import os, pathlib, plistlib, subprocess, sys
root = pathlib.Path(sys.argv[1]); identity, team = sys.argv[2:]
bundles = [root/'Library/Audio/Plug-Ins/VST3/ZED.vst3', root/'Library/Audio/Plug-Ins/Components/ZED.component']
def run(*args):
    return subprocess.check_output(args, stderr=subprocess.STDOUT).decode()
for outer in bundles:
    code = []
    for p in outer.rglob('*'):
        if p.is_symlink():
            if not p.resolve().is_relative_to(outer.resolve()):
                sys.exit('Escaping bundle symlink: ' + str(p))
            continue
        if p.is_file() and 'Mach-O' in run('/usr/bin/file', '-b', str(p)):
            if run('/usr/bin/lipo', '-archs', str(p)).strip() != 'arm64':
                sys.exit('Non-arm64 nested code: ' + str(p))
            code.append(p)
        elif p.is_dir() and p.suffix in ('.framework', '.app', '.xpc', '.bundle', '.vst3', '.component'):
            code.append(p)
    code.append(outer)
    code.sort(key=lambda p: len(p.parts), reverse=True)
    for p in code:
        ent = subprocess.run(['/usr/bin/codesign', '-d', '--entitlements', ':-', str(p)], capture_output=True)
        if ent.stdout.strip() and plistlib.loads(ent.stdout):
            sys.exit('Existing entitlements require review before signing: ' + str(p))
        print('Signing: ' + str(p), flush=True)
        signed = subprocess.run(['/usr/bin/codesign', '--force', '--sign', identity, '--timestamp', '--options', 'runtime', str(p)])
        if signed.returncode:
            sys.exit('Developer ID signing failed. Review the codesign error above and the certificate trust chain/private-key access in Keychain Access, then rerun. No security settings were changed.')
        subprocess.run(['/usr/bin/codesign', '--verify', '--strict', '--verbose=2', str(p)], check=True)
        details = run('/usr/bin/codesign', '-d', '--verbose=4', str(p))
        if ('Authority=' + identity not in details or 'TeamIdentifier=' + team not in details
                or 'Timestamp=' not in details or '(runtime)' not in details):
            sys.exit('Distribution signature requirements not met: ' + str(p))
        print(details)
    subprocess.run(['/usr/bin/codesign', '--verify', '--deep', '--strict', '--verbose=2', str(outer)], check=True)
    print(run('/usr/bin/codesign', '-d', '-r-', str(outer)))
PY

progress 'Creating non-relocatable component installer'
pkgbuild --analyze --root "$stage" "$run/components.plist"
python3 - "$run/components.plist" <<'PY'
import plistlib, sys
p = sys.argv[1]
with open(p, 'rb') as f: entries = plistlib.load(f)
if len(entries) != 2: sys.exit('Expected exactly two top-level package bundles.')
for item in entries:
    item['BundleIsRelocatable'] = False
with open(p, 'wb') as f: plistlib.dump(entries, f)
PY
mkdir -p "$output"
[ "$(cd "$output" && pwd -P)" = "$output" ] || die 'Release directory changed during execution.'
# Unique external directory prevents partial packages from masquerading as final.
pending=$(mktemp -d "$output/.zed-package.XXXXXXXX")
pkg=$pending/$filename
pkgbuild --root "$stage" --component-plist "$run/components.plist" \
    --identifier "$package_id" --version 1.0.0 --install-location / \
    --ownership recommended --sign "$installer_identity" --timestamp "$pkg"
[ -s "$pkg" ] || die 'Installer is missing or empty.'
pkgutil --check-signature "$pkg" | tee "$run/package-signature.txt"
grep -F "Developer ID Installer:" "$run/package-signature.txt" >/dev/null || die 'Installer signature authority missing.'
pkgutil --payload-files "$pkg" | tee "$run/payload-files.txt"
pkgutil --expand-full "$pkg" "$run/expanded"
python3 - "$stage" "$run/expanded" "$run/payload-files.txt" "$package_id" <<'PY'
import hashlib, pathlib, sys, xml.etree.ElementTree as ET
stage, expanded, listing = map(pathlib.Path, sys.argv[1:4])
allowed = ('Library/Audio/Plug-Ins/VST3/ZED.vst3', 'Library/Audio/Plug-Ins/Components/ZED.component')
for raw in listing.read_text().splitlines():
    p = raw.removeprefix('./').rstrip('/')
    if p in ('', '.'): continue
    if p.startswith('/') or '..' in pathlib.PurePosixPath(p).parts:
        sys.exit('Unsafe payload path: ' + p)
    if not any(p == b or p.startswith(b + '/') or b.startswith(p + '/') for b in allowed):
        sys.exit('Unexpected payload path: ' + p)
info = ET.parse(expanded/'PackageInfo').getroot()
assert info.get('identifier') == sys.argv[4] and info.get('version') == '1.0.0'
assert info.get('install-location') == '/' and info.find('scripts') is None
assert not (expanded/'Scripts').exists()
def inventory(root):
    result = {}
    for p in root.rglob('*'):
        rel = p.relative_to(root).as_posix()
        if p.is_symlink(): result[rel] = ('link', p.readlink().as_posix())
        elif p.is_file():
            if p.suffix.lower() in ('.cpp', '.h', '.log', '.p12', '.pem', '.key', '.csv') or p.name == '.DS_Store':
                sys.exit('Unexpected source/working/credential file: ' + rel)
            result[rel] = ('file', hashlib.sha256(p.read_bytes()).hexdigest())
    return result
assert inventory(stage) == inventory(expanded/'Payload'), 'Extracted payload differs from signed staging tree'
for b in allowed:
    bundle = expanded/'Payload'/b
    for required in ('Contents/Info.plist', 'Contents/PkgInfo', 'Contents/MacOS/ZED', 'Contents/_CodeSignature/CodeResources'):
        assert (bundle/required).is_file(), 'Incomplete bundle: ' + b
print('PASS: package identifier/version/location, two complete bundles, exact signed payload, no install scripts.')
PY
# Hard-link publication fails atomically if the final name exists; never overwrite.
ln "$pkg" "$output/$filename"
rm "$pkg"
rmdir "$pending"
progress "Completed: $output/$filename"
stat -f 'Package size: %z bytes' "$output/$filename"
printf 'Package identifier: %s\nBuild: %s\nStaging: %s\n' "$package_id" "$build" "$stage"
printf 'Not notarized, stapled or installed. Retained working files: %s\n' "$run"
