#!/bin/sh
# Host checks for the per-app network choice and the product wiring.
# Compiles the policy classes only. It does not build an APK, boot
# Cuttlefish, or open a socket.
set -eu
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

javac -encoding UTF-8 --release 17 -Xlint:all -Werror -d "$tmp" \
    network/src/org/eleos/network/PolicyFormatException.java \
    network/src/org/eleos/network/NetworkPolicy.java \
    network/src/org/eleos/network/AppRecord.java \
    network/src/org/eleos/network/AppNetwork.java \
    network/src/org/eleos/network/NetworkApps.java \
    network/test/org/eleos/network/NetworkPolicyTest.java
java -cp "$tmp" org.eleos.network.NetworkPolicyTest

for f in network/src/org/eleos/network/*.java; do
    case "$f" in
        *NetworkPermissionActivity.java) continue ;;
    esac
    if grep -q '^import android\.' "$f"; then
        echo "$f imports android" >&2
        exit 1
    fi
done

if grep -R -n -E 'iptables|nftables|VpnService|Runtime\.getRuntime|ProcessBuilder' network; then
    echo 'untested packet filter in the network prototype' >&2
    exit 1
fi

if grep -R -n -i -E 'pixel|shiba' network ele_os_gsi_arm64.mk; then
    echo 'phone-specific target in the network toggle' >&2
    exit 1
fi

if grep -R -n -E 'paddingLeft|paddingRight|layout_marginLeft|layout_marginRight|gravity="[^"]*\b(left|right)\b' network/res; then
    echo 'layout uses left/right instead of start/end' >&2
    exit 1
fi

grep -q 'supportsRtl="true"' network/AndroidManifest.xml
grep -q 'EleNetwork' network/ele_privacy.mk
grep -q 'network/ele_privacy.mk' ele_os_cf_x86_64.mk
grep -q 'network/ele_privacy.mk' ele_os_gsi_arm64.mk
grep -q 'PRODUCT_NAME := ele_os_gsi_arm64' ele_os_gsi_arm64.mk
grep -q 'PRODUCT_DEVICE := generic_arm64' ele_os_gsi_arm64.mk
grep -q 'gsi_release.mk' ele_os_gsi_arm64.mk
grep -q 'ele_os_gsi_arm64.mk' AndroidProducts.mk
grep -q 'ele_os_cf_build.mk' AndroidProducts.mk
grep -q 'ele_os_cf_x86_64-aosp_current-userdebug' AndroidProducts.mk
grep -q 'ele_os_gsi_arm64-aosp_current-userdebug' AndroidProducts.mk

pkgs=$(grep -R -l 'PRODUCT_PACKAGES += EleNetwork' --include='*.mk' .)
if [ "$pkgs" != "./network/ele_privacy.mk" ]; then
    echo "EleNetwork package list is $pkgs" >&2
    exit 1
fi

python3 - <<'PY'
import re
from pathlib import Path
root = Path("network")
strings = (root / "res/values/strings.xml").read_text()
names = set(re.findall(r'<string name="([a-z0-9_]+)"', strings))
needed = {
    "Prototype.",
    "see the network as down",
    "has not been shown to block traffic",
    "cellular baseband",
}
for text in needed:
    if text not in strings:
        raise SystemExit(f"missing prototype wording: {text}")
for layout in (root / "res").rglob("*.xml"):
    for ref in re.findall(r"@string/([A-Za-z0-9_]+)", layout.read_text()):
        if ref not in names:
            raise SystemExit(f"{layout} references missing string {ref}")
    for raw in re.findall(r'android:text="([^"]+)"', layout.read_text()):
        if not raw.startswith("@string/"):
            raise SystemExit(f"{layout} has hardcoded text {raw}")
manifest = (root / "AndroidManifest.xml").read_text()
for ref in re.findall(r"@string/([A-Za-z0-9_]+)", manifest):
    if ref not in names:
        raise SystemExit(f"manifest references missing string {ref}")
print("Network product wiring checks passed")
PY
