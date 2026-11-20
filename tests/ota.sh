#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
if grep -R -i -n -E 'pixel|shiba|husky|shusky' ota tests/ota.cpp .github/workflows/ota-prototype.yml; then
    echo 'phone-maker names do not belong in the update prototype' >&2
    exit 1
fi
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
cxx=${CXX:-c++}
"$cxx" -std=c++17 -Wall -Wextra -Werror -Iota ota/metadata.cpp ota/sign_main.cpp -lcrypto -o "$tmp/ele-ota-sign"
"$cxx" -std=c++17 -Wall -Wextra -Werror -Iota ota/metadata.cpp tests/ota.cpp -lcrypto -o "$tmp/ota-test"
"$tmp/ota-test"
printf 'fixture-payload\n' > "$tmp/payload"

sign() {
    "$tmp/ele-ota-sign" \
        --payload "$tmp/payload" \
        --out-dir "$1" \
        --target "$2" \
        --branch-phase "$3" \
        --version-code "$4" \
        --platform-source "$5"
}

sign "$tmp/cf" cuttlefish maintained 1 aosp-tag:android-16.0.0_r1 > "$tmp/cf.out"
grep -Fxq 'release-key=no' "$tmp/cf.out"
grep -Fxq 'installed=no' "$tmp/cf.out"
grep -Fxq 'status=signed-prototype' "$tmp/cf.out"
grep -Fxq 'channel=test' "$tmp/cf/metadata.txt"
grep -Fxq 'prototype=true' "$tmp/cf/metadata.txt"
grep -Fxq 'product=ele_os_cf_x86_64' "$tmp/cf/metadata.txt"
grep -Fxq 'target=cuttlefish' "$tmp/cf/metadata.txt"
grep -Fxq 'firmware-note=not-claimed' "$tmp/cf/metadata.txt"
grep -Fxq 'vendor-partition=not-in-payload' "$tmp/cf/metadata.txt"
grep -Fxq 'payload-kind=virtual-device-fixture' "$tmp/cf/metadata.txt"
test "$(wc -c < "$tmp/cf/metadata.sig")" -eq 64

sign "$tmp/gsi" treble-gsi extended 3 fixes-only > "$tmp/gsi.out"
grep -Fxq 'target=treble-gsi' "$tmp/gsi/metadata.txt"
grep -Fxq 'product=ele_os_gsi_arm64' "$tmp/gsi/metadata.txt"
grep -Fxq 'branch-phase=extended' "$tmp/gsi/metadata.txt"
grep -Fxq 'platform-source=fixes-only' "$tmp/gsi/metadata.txt"
grep -Fxq 'payload-kind=system-image-fixture' "$tmp/gsi/metadata.txt"
grep -Fxq 'vendor-partition=not-in-payload' "$tmp/gsi/metadata.txt"
grep -Fxq 'installed=no' "$tmp/gsi.out"

openssl genpkey -algorithm ED25519 -out "$tmp/wrong.pem"
if "$tmp/ele-ota-sign" \
    --key "$tmp/wrong.pem" \
    --payload "$tmp/payload" \
    --out-dir "$tmp/wrong-out" \
    --target cuttlefish \
    --branch-phase maintained \
    --version-code 1 \
    --platform-source aosp-tag:android-16.0.0_r1 2>"$tmp/wrong.err"; then
    echo 'expected the non-test key to be refused' >&2
    exit 1
fi
grep -Fxq 'refusing key that is not the repository test key' "$tmp/wrong.err"

if "$tmp/ele-ota-sign" \
    --payload "$tmp/payload" \
    --out-dir "$tmp/bad-phase" \
    --target cuttlefish \
    --branch-phase extended \
    --version-code 1 \
    --platform-source aosp-tag:android-16.0.0_r1 2>"$tmp/bad-phase.err"; then
    echo 'expected an extended package with a new tag to be refused' >&2
    exit 1
fi
grep -Fxq 'extended packages list fixes only' "$tmp/bad-phase.err"
echo 'OTA signing checks passed'
