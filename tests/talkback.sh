#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
# Host check for the TalkBack pin and for product membership.
# Does not fetch upstream and does not build an APK.
set -eu
cd "$(dirname "$0")/.."

fail() {
    echo "talkback check failed: $*" >&2
    exit 1
}

. ./talkback/PIN

[ "$upstream_url" = "https://github.com/google/talkback" ] || fail "url"
[ "$revision" = "229212fdf5842191d0a93fc95d9ca1423b346866" ] || fail "revision"
[ "$version_name" = "TfPu_release_16_2" ] || fail "version"
[ "$spdx" = "Apache-2.0" ] || fail "spdx"
[ "$license_sha256" = "58d1e17ffe5109a7ae296caafcadfdbe6a7d176f0bc4ab01e12a689b0499d8bd" ] || fail "sha256"
[ "$license_git_blob" = "7a4a3ea2424c09fbe48d455aed1eaa94d9124835" ] || fail "blob"
[ "$application_id" = "com.android.talkback" ] || fail "application id"
[ "$flavor" = "phone" ] || fail "flavor"

grep -q "revision=\"$revision\"" talkback/ele-talkback.xml || fail "manifest revision"
grep -q 'name="google/talkback"' talkback/ele-talkback.xml || fail "manifest name"
grep -q "$revision" talkback/NOTICE || fail "NOTICE revision"
grep -q 'Apache-2.0' talkback/ele_talkback.mk || fail "product license id"

sh -n talkback/sync-pin.sh
sh -n tests/talkback.sh

for phrase in \
    "$revision" \
    "$version_name" \
    "Apache-2.0" \
    "com.google.android.gms:play-services-mlkit-text-recognition:19.0.1" \
    "com.google.android.play:core:1.10.3" \
    "LGPL-2.1-or-later" \
    "GPL-2.0-or-later" \
    "prototype" \
    "ele_os_gsi_arm64" \
    "ele_os_cf_x86_64" \
    "ro.config.low_ram" \
    "armeabi-v7a" \
    "arm64-v8a" \
    "com.android.talkback"
do
    grep -q "$phrase" talkback/README.md || fail "README missing $phrase"
done

if find talkback products -name '*.apk' | grep -q .; then
    fail "an APK is in the tree"
fi

if grep -R -n -E -i --exclude-dir=upstream \
    'pixel|shiba|husky|oriole|shusky' \
    talkback products AndroidProducts.mk
then
    fail "phone-maker target in the TalkBack products"
fi

grep -q 'PRODUCT_PACKAGES += EleTalkBack EleTalkBackNotice' talkback/ele_talkback.mk \
    || fail "package list"
grep -q 'EleTalkBack.apk' talkback/ele_talkback.mk || fail "apk gate"
grep -F -q '$(error' talkback/ele_talkback.mk || fail "configure error"
grep -q 'LOCAL_CERTIFICATE := PRESIGNED' talkback/Android.mk || fail "presigned"
grep -q 'EleTalkBack.apk' talkback/Android.mk || fail "prebuilt src"
grep -q 'name: "EleTalkBackNotice"' talkback/Android.bp || fail "notice module"

# device/ele/os/PATH -> PATH, for inherit-product lines in this overlay.
inherits_of() {
    sed -n 's/.*device\/ele\/os\///p' "$1" | tr -d ' )'
}

reaches_talkback() {
    f=$1
    case " $seen " in
        *" $f "*) return 1 ;;
    esac
    seen="$seen $f"
    if [ "$f" = "talkback/ele_talkback.mk" ]; then
        return 0
    fi
    [ -f "$f" ] || return 1
    for dep in $(inherits_of "$f"); do
        if reaches_talkback "$dep"; then
            return 0
        fi
    done
    return 1
}

product_name_of() {
    f=$1
    if grep -q '^PRODUCT_NAME := ' "$f"; then
        sed -n 's/^PRODUCT_NAME := //p' "$f" | tail -n 1
        return
    fi
    for dep in $(inherits_of "$f"); do
        if [ -f "$dep" ] && grep -q '^PRODUCT_NAME := ' "$dep"; then
            sed -n 's/^PRODUCT_NAME := //p' "$dep" | tail -n 1
            return
        fi
    done
    fail "no PRODUCT_NAME in $f"
}

grep -q 'ele_os_cf_x86_64.mk' AndroidProducts.mk || fail "cf product missing"
if grep -E 'LOCAL_DIR\)/ele_os_cf_x86_64\.mk' AndroidProducts.mk; then
    fail "raw Cuttlefish makefile is the lunch entry"
fi

lowram=0
gsi=0
cf=0
set -- $(sed -n 's/.*\$(LOCAL_DIR)\///p' AndroidProducts.mk | tr -d ' \\')
[ "$#" -ge 4 ] || fail "expected four products"
for product in "$@"; do
    [ -f "$product" ] || fail "missing $product"
    seen=
    reaches_talkback "$product" || fail "$product does not inherit TalkBack"
    name=$(product_name_of "$product") || fail "PRODUCT_NAME for $product"
    grep -q "$name-trunk_staging-userdebug" AndroidProducts.mk \
        || fail "lunch choice for $name"
    case "$name" in
        *lowram*)
            lowram=$((lowram + 1))
            grep -q 'ro.config.low_ram=true' "$product" || fail "$name low_ram"
            ;;
        *)
            if grep -q 'ro.config.low_ram=true' "$product"; then
                fail "$name sets low_ram"
            fi
            ;;
    esac
    case "$product" in
        *gsi*)
            gsi=$((gsi + 1))
            ;;
        *cf*)
            cf=$((cf + 1))
            ;;
    esac
done

[ "$lowram" -ge 2 ] || fail "low-RAM products"
[ "$gsi" -ge 2 ] || fail "GSI products"
[ "$cf" -ge 2 ] || fail "Cuttlefish products"
grep -q 'generic_arm64' products/ele_os_gsi_arm64.mk || fail "generic arm64"
grep -q 'gsi_release.mk' products/ele_os_gsi_arm64.mk || fail "gsi release"
grep -q 'cuttlefish' ele_os_cf_x86_64.mk || fail "cuttlefish base"

echo "TalkBack pin and product checks passed"
