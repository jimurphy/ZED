#!/bin/bash
# Bash 3.2; authenticate only with an existing notarytool Keychain profile.
set -euo pipefail
umask 077
export LC_ALL=C
trap 'printf "ERROR: command failed at line %s; inspect retained external logs.\n" "$LINENO" >&2' ERR
die() { printf 'ERROR: %s\n' "$*" >&2; exit 1; }
progress() { printf '\n==> %s\n' "$*"; }
package=${ZED_PACKAGE_PATH:-}
profile=${ZED_NOTARY_PROFILE:-ZED-notary}
mode=normal
while [ "$#" -gt 0 ]; do
    case "$1" in
        --package|--profile)
            [ "$#" -ge 2 ] && [ -n "$2" ] || die "$1 requires a value."
            if [ "$1" = --package ]; then package=$2; else profile=$2; fi
            shift 2 ;;
        --validate-only|--staple-only)
            [ "$mode" = normal ] || die 'Choose only one operating mode.'
            mode=${1#--}; shift ;;
        --help)
            cat <<'HELP'
Usage: scripts/macos/notarize-package.sh --package /external/ZED.pkg [options]
  --profile NAME    Existing Keychain profile (default: ZED-notary)
  --validate-only   Require a stapled ticket; never submit or modify the package
  --staple-only     Recover an accepted submission; never submit again
Environment alternatives: ZED_PACKAGE_PATH, ZED_NOTARY_PROFILE.
No password arguments are supported. Successful validation writes .pkg.sha256.
HELP
            exit 0 ;;
        *) die "Unknown argument: $1 (use --help)." ;;
    esac
done
[ "$(uname -s)" = Darwin ] || die 'Requires macOS.'
[ -n "$package" ] || die 'Supply --package or ZED_PACKAGE_PATH.'
[ -n "$profile" ] || die 'Keychain profile must not be empty.'
case "$package" in /*) ;; *) package=$PWD/$package ;; esac
# Resolve directory symlinks; refuse a symlink in place of the package itself.
[ ! -L "$package" ] || die 'Supply the actual package, not a package symlink.'
[ -f "$package" ] && [ -s "$package" ] || die 'Package must be a regular, non-empty file.'
directory=$(cd "$(dirname "$package")" && pwd -P)
filename=$(basename "$package")
case "$filename" in *.pkg) ;; *) die 'Expected a .pkg file.' ;; esac
case "$filename" in *$'\n'*|*$'\r'*|*\\*) die 'Unsupported filename for checksum output.' ;; esac
package=$directory/$filename
script_dir=$(cd "$(dirname "$0")" && pwd -P)
repo=$(git -C "$script_dir" rev-parse --show-toplevel)
repo=$(cd "$repo" && pwd -P)
case "$package" in "$repo"/*) die 'Release package must be outside the Git worktree.' ;; esac
for tool in /usr/bin/xcrun /usr/sbin/pkgutil /usr/sbin/spctl /usr/bin/plutil /usr/bin/shasum; do
    [ -x "$tool" ] || die "Missing required macOS tool: $tool"
done
/usr/bin/xcrun --find stapler >/dev/null
checksum=$package.sha256
[ ! -L "$checksum" ] || die 'Refusing to replace a checksum symlink.'
[ ! -e "$checksum" ] || [ -f "$checksum" ] || die 'Checksum destination is not a regular file.'
logs=$directory/.notarization
[ ! -L "$logs" ] || die 'Refusing a symlinked .notarization directory.'
mkdir -p "$logs"
[ "$(cd "$logs" && pwd -P)" = "$logs" ] || die 'Unexpected log directory resolution.'
run=$(mktemp -d "$logs/run.XXXXXXXX")
mkdir "$run/tmp"
export TMPDIR=$run/tmp
progress "Package: $package"
stat -f 'Size: %z bytes' "$package"
printf 'Profile: %s\nMode: %s\nLogs: %s\n' "$profile" "$mode" "$run"
hash_package() { /usr/bin/shasum -a 256 "$package" | awk '{print $1}'; }
initial_hash=$(hash_package)
check_signature() {
    local log=$1
    if /usr/sbin/pkgutil --check-signature "$package" >"$log" 2>&1; then
        cat "$log"
    else
        cat "$log" >&2
        die "Installer signature check failed; see $log"
    fi
    grep -Eq '^[[:space:]]*Status: (signed by a certificate trusted by (Mac OS X|macOS)|signed by a developer certificate issued by Apple for distribution)$' "$log" || die 'Installer signature is not trusted.'
    grep -Eq '^[[:space:]]*1\. Developer ID Installer:' "$log" || die 'Signer is not Developer ID Installer.'
}
check_signature "$run/signature-before.txt"

already_stapled=false
if /usr/bin/xcrun stapler validate "$package" >"$run/stapler-before.txt" 2>&1; then
    already_stapled=true
    cat "$run/stapler-before.txt"
    progress 'Already stapled: skipping submission and stapling.'
else
    cat "$run/stapler-before.txt"
    [ "$mode" != validate-only ] || die 'Validate-only requires a valid existing stapled ticket; package left unchanged.'
fi

if [ "$already_stapled" = false ]; then
    if [ "$mode" = normal ]; then
        /usr/bin/xcrun --find notarytool >/dev/null
        progress 'Checking Keychain profile with notarytool history'
        if ! /usr/bin/xcrun notarytool history --keychain-profile "$profile" --output-format json \
            >"$run/history.json" 2>"$run/history-error.txt"; then
            cat "$run/history-error.txt" >&2
            die "Profile/network check failed; no submission made. See $run"
        fi
        progress 'Submitting and waiting for Apple (no automatic retries)'
        submit_rc=0
        /usr/bin/xcrun notarytool submit "$package" --keychain-profile "$profile" \
            --wait --output-format json >"$run/submission.json" 2>"$run/submission-error.txt" || submit_rc=$?
        submission_id=$(/usr/bin/plutil -extract id raw -o - "$run/submission.json" 2>/dev/null) || submission_id=
        status=$(/usr/bin/plutil -extract status raw -o - "$run/submission.json" 2>/dev/null) || status=
        printf 'Submission ID: %s\nFinal status: %s\n' "${submission_id:-unavailable}" "${status:-unavailable}"
        [ "$(hash_package)" = "$initial_hash" ] || die 'Package changed during submission; refusing to staple.'
        if [ "$submit_rc" -ne 0 ] || [ "$status" != Accepted ] || [ -z "$submission_id" ]; then
            cat "$run/submission-error.txt" >&2
            if [ -n "$submission_id" ]; then
                if /usr/bin/xcrun notarytool log "$submission_id" "$run/notarization-log.json" \
                    --keychain-profile "$profile" >"$run/log-fetch.txt" 2>&1; then
                    printf 'Notarization log: %s\n' "$run/notarization-log.json" >&2
                else
                    cat "$run/log-fetch.txt" >&2
                    printf 'Could not retrieve log; retain submission ID %s for recovery.\n' "$submission_id" >&2
                fi
            fi
            die "Notarization did not complete successfully (exit $submit_rc). Package was not stapled. Inspect $run before retrying; Apple may still be processing."
        fi
    fi
    # staple-only deliberately relies on Apple's available ticket, without a new submission.
    progress 'Stapling the accepted ticket'
    if ! /usr/bin/xcrun stapler staple "$package" >"$run/staple.txt" 2>&1; then
        cat "$run/staple.txt" >&2
        die "Stapling failed. Keep logs in $run; retry with --staple-only, not a new submission."
    fi
    cat "$run/staple.txt"
fi

progress 'Final ticket, Gatekeeper and Installer signature validation'
if ! /usr/bin/xcrun stapler validate "$package" >"$run/stapler-final.txt" 2>&1; then
    cat "$run/stapler-final.txt" >&2; die 'Final stapler validation failed.'
fi
cat "$run/stapler-final.txt"
if ! /usr/sbin/spctl --assess --type install --verbose=4 "$package" >"$run/gatekeeper.txt" 2>&1; then
    cat "$run/gatekeeper.txt" >&2; die 'Gatekeeper assessment failed.'
fi
cat "$run/gatekeeper.txt"
grep -Fx 'source=Notarized Developer ID' "$run/gatekeeper.txt" >/dev/null || die 'Gatekeeper did not identify Notarized Developer ID.'
check_signature "$run/signature-final.txt"
final_hash=$(hash_package)
if [ "$mode" = validate-only ] || [ "$already_stapled" = true ]; then
    [ "$initial_hash" = "$final_hash" ] || die 'Package changed during read-only validation; checksum not published.'
fi
# Publish only after every check; never modify the package after this point.
printf '%s  %s\n' "$final_hash" "$filename" >"$run/checksum.sha256"
mv -f "$run/checksum.sha256" "$checksum"
progress "Validated. Checksum: $checksum"
printf '%s  %s\n' "$final_hash" "$filename"
printf 'Logs retained: %s\nNo installation performed.\n' "$run"
