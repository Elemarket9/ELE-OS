#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
if grep -n -E 'installed=yes|update_engine|https?://' ota/client.cpp ota/client_main.cpp ota/client.h; then
    echo 'the client must not install, call update_engine, or name a server' >&2
    exit 1
fi
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
cxx=${CXX:-c++}
"$cxx" -std=c++17 -Wall -Wextra -Werror -Iota ota/metadata.cpp ota/sign_main.cpp -lcrypto -o "$tmp/ele-ota-sign"
"$cxx" -std=c++17 -Wall -Wextra -Werror -Iota ota/metadata.cpp ota/client.cpp ota/client_main.cpp -lcrypto -o "$tmp/ele-ota-client"
printf 'fixture-payload\n' > "$tmp/payload"

sign() {
    "$tmp/ele-ota-sign" \
        --payload "$tmp/payload" \
        --out-dir "$1" \
        --target "$2" \
        --branch-phase "$3" \
        --version-code "$4" \
        --platform-source "$5" >/dev/null
}

decide() {
    set +e
    "$tmp/ele-ota-client" \
        --channel "$1" \
        --target "$2" \
        --product "$3" \
        --current-version "$4" >"$5" 2>"$5.err"
    status=$?
    set -e
    echo "$status"
}

mkdir -p "$tmp/channel"
sign "$tmp/channel/v1" cuttlefish maintained 1 aosp-tag:android-16.0.0_r1
sign "$tmp/channel/v2" cuttlefish maintained 2 aosp-tag:android-16.0.0_r1

code=$(decide "$tmp/channel" cuttlefish ele_os_cf_x86_64 1 "$tmp/pick")
test "$code" -eq 0
grep -Fxq 'installed=no' "$tmp/pick"
grep -Fxq 'status=verified-not-installed' "$tmp/pick"
grep -Fxq 'package=v2' "$tmp/pick"
grep -Fxq 'version-code=2' "$tmp/pick"
grep -Fxq 'reason=verified-not-installed' "$tmp/pick"
grep -Fxq 'ELE-OS-UPDATE-DECISION/1' "$tmp/pick"

code=$(decide "$tmp/channel" cuttlefish ele_os_cf_x86_64 2 "$tmp/same")
test "$code" -eq 0
grep -Fxq 'status=no-update' "$tmp/same"
grep -Fxq 'installed=no' "$tmp/same"
grep -Fxq 'reason=no-newer-verified-package' "$tmp/same"

code=$(decide "$tmp/channel" cuttlefish ele_os_cf_x86_64 9 "$tmp/older")
test "$code" -eq 0
grep -Fxq 'status=no-update' "$tmp/older"

python3 - "$tmp/channel/v2/metadata.sig" <<'PY'
import sys
path = sys.argv[1]
data = bytearray(open(path, "rb").read())
data[0] ^= 1
open(path, "wb").write(data)
PY
code=$(decide "$tmp/channel" cuttlefish ele_os_cf_x86_64 0 "$tmp/badsig")
test "$code" -eq 1
grep -Fxq 'status=rejected' "$tmp/badsig"
grep -Fxq 'reason=bad-signature' "$tmp/badsig"
grep -Fxq 'installed=no' "$tmp/badsig"
grep -Fxq 'package=-' "$tmp/badsig"
grep -Fxq 'rejected=v2' "$tmp/badsig"

rm -rf "$tmp/channel/v2"
sign "$tmp/channel/v2" cuttlefish maintained 2 aosp-tag:android-16.0.0_r1
printf 'x' >> "$tmp/channel/v2/payload"
code=$(decide "$tmp/channel" cuttlefish ele_os_cf_x86_64 0 "$tmp/badhash")
test "$code" -eq 1
grep -Fxq 'status=rejected' "$tmp/badhash"
grep -Fxq 'reason=unreadable-package' "$tmp/badhash"
grep -Fxq 'installed=no' "$tmp/badhash"

rm -rf "$tmp/channel/v2"
sign "$tmp/channel/v2" cuttlefish maintained 2 aosp-tag:android-16.0.0_r1
sign "$tmp/channel/v2b" cuttlefish maintained 2 aosp-tag:android-16.0.0_r1
code=$(decide "$tmp/channel" cuttlefish ele_os_cf_x86_64 0 "$tmp/ambig")
test "$code" -eq 1
grep -Fxq 'reason=ambiguous-version' "$tmp/ambig"
grep -Fxq 'installed=no' "$tmp/ambig"
grep -Fxq 'rejected=v2,v2b' "$tmp/ambig"

mkdir -p "$tmp/gsi"
sign "$tmp/gsi/g1" treble-gsi extended 4 fixes-only
code=$(decide "$tmp/gsi" cuttlefish ele_os_cf_x86_64 0 "$tmp/other")
test "$code" -eq 0
grep -Fxq 'status=no-update' "$tmp/other"
code=$(decide "$tmp/gsi" treble-gsi ele_os_gsi_arm64 0 "$tmp/gsi-pick")
test "$code" -eq 0
grep -Fxq 'status=verified-not-installed' "$tmp/gsi-pick"
grep -Fxq 'product=ele_os_gsi_arm64' "$tmp/gsi-pick"
grep -Fxq 'installed=no' "$tmp/gsi-pick"

if "$tmp/ele-ota-client" \
    --channel "$tmp/channel" \
    --target cuttlefish \
    --product ele_os_gsi_arm64 \
    --current-version 0 >"$tmp/mismatch" 2>"$tmp/mismatch.err"; then
    echo 'expected a target and product mismatch to fail' >&2
    exit 1
fi
grep -Fxq 'product does not match target' "$tmp/mismatch.err"
grep -Fxq 'installed=no' "$tmp/mismatch"

openssl genpkey -algorithm ED25519 -out "$tmp/wrong.pem"
if "$tmp/ele-ota-client" \
    --public "$tmp/wrong.pem" \
    --channel "$tmp/channel" \
    --target cuttlefish \
    --product ele_os_cf_x86_64 \
    --current-version 0 >"$tmp/wrong.out" 2>"$tmp/wrong.err"; then
    echo 'expected a non-test public key to be refused' >&2
    exit 1
fi
grep -Fxq 'refusing key that is not the repository test key' "$tmp/wrong.err"
grep -Fxq 'installed=no' "$tmp/wrong.out"
echo 'OTA client checks passed'
