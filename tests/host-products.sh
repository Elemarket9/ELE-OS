#!/bin/sh
# Host check for the class products. No AOSP tree, no emulator, no phone.
# SPDX-License-Identifier: Apache-2.0
set -eu
cd "$(dirname "$0")/.."

fail() {
    printf 'host-products: %s\n' "$1" >&2
    exit 1
}

for f in \
    ele_os_cf_build.mk \
    ele_os_cf_x86_64.mk \
    ele_os_cf_x86_64_low_ram.mk \
    ele_os_gsi_arm64.mk \
    ele_os_gsi_low_ram_arm64.mk \
    products/common.mk \
    products/low_ram.mk \
    AndroidProducts.mk
do
    [ -f "$f" ] || fail "missing $f"
done

grep -q 'ele_os_cf_x86_64.mk' ele_os_cf_build.mk || fail 'Cuttlefish build contract does not inherit the base'
grep -q 'products/common.mk' ele_os_cf_build.mk || fail 'Cuttlefish build contract skips the shared fragment'
grep -q 'low_ram.mk' ele_os_cf_build.mk && fail 'default Cuttlefish product inherits the low-RAM class'

grep -q 'products/low_ram.mk' ele_os_cf_x86_64_low_ram.mk || fail 'Cuttlefish low-RAM product misses the class fragment'
grep -q 'products/common.mk' ele_os_cf_x86_64_low_ram.mk || fail 'Cuttlefish low-RAM product skips the shared fragment'
grep -q 'PRODUCT_NAME := ele_os_cf_x86_64_low_ram' ele_os_cf_x86_64_low_ram.mk || fail 'Cuttlefish low-RAM product name'

grep -q 'product/aosp_arm64.mk' ele_os_gsi_arm64.mk || fail 'GSI does not inherit the AOSP arm64 GSI'
grep -q 'product/gsi_release.mk' ele_os_gsi_arm64.mk || fail 'GSI does not inherit gsi_release'
grep -q 'products/common.mk' ele_os_gsi_arm64.mk || fail 'GSI skips the shared fragment'
grep -q 'low_ram.mk' ele_os_gsi_arm64.mk && fail 'plain GSI inherits the low-RAM class'
grep -q 'product/aosp_arm.mk' ele_os_gsi_arm64.mk && fail '32-bit GSI was added without a porter device'
grep -q 'device/google/' ele_os_gsi_arm64.mk && fail 'GSI points at a device/google tree'
grep -q 'device/google/' ele_os_gsi_low_ram_arm64.mk && fail 'low-RAM GSI points at a device/google tree'

grep -q 'ele_os_gsi_arm64.mk' ele_os_gsi_low_ram_arm64.mk || fail 'low-RAM GSI does not inherit the plain GSI'
grep -q 'products/low_ram.mk' ele_os_gsi_low_ram_arm64.mk || fail 'low-RAM GSI misses the class fragment'
grep -q 'PRODUCT_NAME := ele_os_gsi_low_ram_arm64' ele_os_gsi_low_ram_arm64.mk || fail 'low-RAM GSI product name'

grep -q 'ro.config.low_ram=true' products/low_ram.mk || fail 'low-RAM class does not set the AOSP switch'
grep -q 'ro.config.low_ram=false' products/low_ram.mk && fail 'low-RAM class clears the switch'
grep -q 'UNMEASURED' products/low_ram.mk || fail 'low-RAM class file is not marked unmeasured'
if grep -E '^[^#]*inherit-product.*go_defaults' products/low_ram.mk; then
    fail 'low-RAM class inherits Android Go defaults'
fi

# The switch lives only in the class fragment.
for f in ele_os_cf_build.mk ele_os_cf_x86_64.mk ele_os_cf_x86_64_low_ram.mk \
    ele_os_gsi_arm64.mk ele_os_gsi_low_ram_arm64.mk products/common.mk
do
    if grep -q 'ro.config.low_ram' "$f"; then
        fail "$f sets ro.config.low_ram; only products/low_ram.mk may"
    fi
done

for lunch in \
    ele_os_cf_x86_64-aosp_current-userdebug \
    ele_os_cf_x86_64_low_ram-aosp_current-userdebug \
    ele_os_gsi_arm64-aosp_current-userdebug \
    ele_os_gsi_low_ram_arm64-aosp_current-userdebug
do
    grep -q "$lunch" AndroidProducts.mk || fail "lunch choice missing: $lunch"
done

grep -q 'ele_os_cf_build.mk' AndroidProducts.mk || fail 'AndroidProducts does not list the Cuttlefish build contract'
if grep -F '$(LOCAL_DIR)/ele_os_cf_x86_64.mk' AndroidProducts.mk; then
    fail 'AndroidProducts lists the base Cuttlefish makefile directly'
fi

# No named-phone target in the class files.
for f in ele_os_cf_build.mk ele_os_cf_x86_64_low_ram.mk ele_os_gsi_arm64.mk \
    ele_os_gsi_low_ram_arm64.mk products/low_ram.mk products/common.mk \
    AndroidProducts.mk
do
    if grep -E -q 'Pixel|shiba|husky' "$f"; then
        fail "$f names a phone"
    fi
done

# Optional second argument defaults to the existing Cuttlefish product.
grep -q 'ele_os_cf_x86_64' tests/check-boot.sh || fail 'check-boot lost the default product'
grep -q 'product=${product_name}' tests/check-boot.sh || fail 'check-boot does not take a product name'

echo 'host-products: ok'
