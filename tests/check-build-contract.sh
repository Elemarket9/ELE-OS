#!/bin/sh
# Static checks for the prototype build contract.
# Does not sync AOSP, compile an image, boot Cuttlefish, or sign anything.
set -eu
cd "$(dirname "$0")/.."

fail() {
    printf 'check-build-contract: %s\n' "$1" >&2
    exit 1
}

command -v python3 >/dev/null 2>&1 || fail "python3 is required"

pin=$(tr -d '[:space:]' < manifest/aosp-tag)
[ "$pin" = "android-16.0.0_r1" ] || fail "manifest/aosp-tag must be android-16.0.0_r1"
grep -q "$pin" manifest/ele-os-local.xml || fail "local manifest does not name the pin"

python3 - << 'PY'
import pathlib
import sys
import xml.etree.ElementTree as ET

root = ET.parse("manifest/ele-os-local.xml").getroot()
if root.tag != "manifest":
    sys.exit("manifest root element must be manifest")
if root.findall("remove-project"):
    sys.exit("local manifest must not remove upstream projects")
projects = root.findall("project")
if len(projects) != 1:
    sys.exit("local manifest must add exactly one project")
project = projects[0]
expected = {
    "name": "Elemarket9/ELE-OS",
    "path": "device/ele/os",
    "remote": "github",
    "revision": "refs/heads/main",
}
for key, value in expected.items():
    if project.get(key) != value:
        sys.exit(f"project {key} must be {value}")
for element in root.iter():
    path = element.get("path") or ""
    if path.startswith("device/") and path != "device/ele/os":
        sys.exit(f"unexpected device project path {path}")
remotes = root.findall("remote")
if len(remotes) != 1 or remotes[0].get("name") != "github":
    sys.exit("local manifest must declare one github remote")
if remotes[0].get("fetch") != "https://github.com":
    sys.exit("github remote fetch must be https://github.com")
text = pathlib.Path("manifest/ele-os-local.xml").read_text()
if "PROTOTYPE" not in text:
    sys.exit("local manifest must stay marked as a prototype")
PY

grep -q 'PRODUCT_MAKEFILES := $(LOCAL_DIR)/ele_os_cf_build.mk' AndroidProducts.mk \
    || fail "AndroidProducts.mk must register ele_os_cf_build.mk"
grep -q 'COMMON_LUNCH_CHOICES := ele_os_cf_x86_64-aosp_current-userdebug' AndroidProducts.mk \
    || fail "lunch combo must be ele_os_cf_x86_64-aosp_current-userdebug"
if grep -E '^[[:space:]]*COMMON_LUNCH_CHOICES' AndroidProducts.mk | grep -q 'trunk_staging'; then
    fail "AndroidProducts.mk must not register trunk_staging"
fi
if grep -R -n --exclude-dir=.git --exclude=check-build-contract.sh \
    -F 'ele_os_cf_x86_64-trunk_staging-userdebug' . >/dev/null; then
    fail "docs still tell lunch to use trunk_staging"
fi
grep -q 'ele_os_cf_x86_64-aosp_current-userdebug' docs/build.md \
    || fail "docs/build.md must name the registered lunch combo"
grep -q 'ele_os_cf_x86_64-aosp_current-userdebug' docs/architecture.md \
    || fail "docs/architecture.md must name the registered lunch combo"

grep -q 'inherit-product, $(LOCAL_DIR)/ele_os_cf_x86_64.mk' ele_os_cf_build.mk \
    || fail "build contract must inherit the Cuttlefish identity makefile"
grep -q 'PROTOTYPE' ele_os_cf_build.mk \
    || fail "build contract makefile must stay marked as a prototype"
grep -q 'PRODUCT_MANUFACTURER := ELE-OS' ele_os_cf_build.mk \
    || fail "virtual product manufacturer must be ELE-OS"
grep -q 'PRODUCT_DEFAULT_DEV_CERTIFICATE := build/make/target/product/security/testkey' ele_os_cf_build.mk \
    || fail "dev certificate must be the AOSP test key"
grep -q 'ro.eleos.keys=test' ele_os_cf_build.mk \
    || fail "image label ro.eleos.keys=test is required"
grep -q 'ro.eleos.prototype=1' ele_os_cf_build.mk \
    || fail "image label ro.eleos.prototype=1 is required"

grep -q '^PRODUCT_NAME := ele_os_cf_x86_64$' ele_os_cf_x86_64.mk \
    || fail "identity makefile must still name ele_os_cf_x86_64"
grep -q 'device/google/cuttlefish/vsoc_x86_64_only/phone/aosp_cf.mk' ele_os_cf_x86_64.mk \
    || fail "identity makefile must inherit the AOSP Cuttlefish phone"
grep -q '^PRODUCT_DEVICE := vsoc_x86_64_only$' ele_os_cf_x86_64.mk \
    || fail "identity makefile must stay on the Cuttlefish device"

# Certificate and package guards apply to makefiles we control. The identity
# makefile is included so a GMS inherit cannot land there unnoticed.
for file in ele_os_cf_build.mk AndroidProducts.mk ele_os_cf_x86_64.mk; do
    if grep -E '^[[:space:]]*PRODUCT_DEFAULT_DEV_CERTIFICATE[[:space:]]*[:?+]?=' "$file" \
        | grep -v 'build/make/target/product/security/testkey$' >/dev/null; then
        fail "$file assigns a dev certificate other than the AOSP test key"
    fi
    if grep -E '^[[:space:]]*(PRODUCT_VERITY_SIGNING_KEY|PRODUCT_EXTRA_RECOVERY_KEYS)[[:space:]]*[:?+]?=' "$file" >/dev/null; then
        fail "$file assigns a release signing key"
    fi
    if grep -E '^[[:space:]]*PRODUCT_PACKAGES[[:space:]]*[+:?]?=.*\<(Phonesky|Velvet|GmsCore|PrebuiltGmsCore|GoogleServicesFramework)\>' "$file" >/dev/null; then
        fail "$file installs a Google package"
    fi
done

if find . -path './.git' -prune -o -type f \( \
    -name '*.pk8' -o -name '*.pem' -o -name '*.apk' -o -name '*.so' -o -name '*.img' \
    \) -print | grep -q .; then
    fail "refusing private keys or prebuilt blobs in this tree"
fi

if grep -R -n --exclude-dir=.git --exclude=check-build-contract.sh \
    -F 'BEGIN PRIVATE KEY' . >/dev/null; then
    fail "refusing a private key block"
fi

printf 'Build contract checks passed (manifest and makefiles only; AOSP was not built)\n'
