#!/bin/zsh

set -euo pipefail

usage() {
    cat <<'EOF'
Usage:
  CODESIGN_IDENTITY="Developer ID Application: Example" \
  NOTARYTOOL_PROFILE="notary-profile" \
  ./scripts/macos/sign_and_notarize.sh path/to/cpp-audio-converter.app

Environment:
  CODESIGN_IDENTITY   Required. Developer ID Application identity name.
  NOTARYTOOL_PROFILE  Optional. Keychain profile for xcrun notarytool.

The script signs nested dylibs and executables, signs the app bundle, verifies the
signature, and if NOTARYTOOL_PROFILE is set, submits the zipped app for notarization
and staples the notarization ticket.
EOF
}

if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
    usage
    exit 0
fi

if [[ $# -ne 1 ]]; then
    usage >&2
    exit 1
fi

if [[ -z "${CODESIGN_IDENTITY:-}" ]]; then
    echo "CODESIGN_IDENTITY is required." >&2
    exit 1
fi

app_path="${1:A}"
if [[ ! -d "${app_path}" ]]; then
    echo "App bundle not found: ${app_path}" >&2
    exit 1
fi

sign_target() {
    local target_path="$1"
    /usr/bin/codesign --force --timestamp --options runtime --sign "${CODESIGN_IDENTITY}" "${target_path}"
}

sign_matching_files() {
    local base_path="$1"
    shift

    if [[ ! -d "${base_path}" ]]; then
        return
    fi

    while IFS= read -r -d '' item; do
        sign_target "${item}"
    done < <(/usr/bin/find "${base_path}" "$@" -print0 | /usr/bin/sort -z)
}

sign_matching_files "${app_path}/Contents/Frameworks" \
    \( -type f -a \( -name '*.dylib' -o -name '*.so' -o -perm -111 \) \)
sign_matching_files "${app_path}/Contents/PlugIns" \
    \( -type f -a -perm -111 \)
sign_matching_files "${app_path}/Contents/MacOS" \
    \( -type f -a -perm -111 \)

sign_target "${app_path}"
/usr/bin/codesign --verify --deep --strict --verbose=2 "${app_path}"

if [[ -z "${NOTARYTOOL_PROFILE:-}" ]]; then
    echo "Signed ${app_path}"
    exit 0
fi

zip_path="${app_path:h}/${app_path:t:r}.zip"
/usr/bin/ditto -c -k --norsrc --keepParent "${app_path}" "${zip_path}"
/usr/bin/xcrun notarytool submit "${zip_path}" --keychain-profile "${NOTARYTOOL_PROFILE}" --wait
/usr/bin/xcrun stapler staple "${app_path}"
/usr/sbin/spctl --assess --type execute --verbose=4 "${app_path}"

echo "Signed and notarized ${app_path}"
