#!/bin/sh
# The memory and battery fragments are config, not a measured result.
# SPDX-License-Identifier: Apache-2.0
set -eu
cd "$(dirname "$0")/.."

fail() {
    printf 'host-tuning: %s\n' "$1" >&2
    exit 1
}

memory=products/memory_low_ram.mk
battery=products/battery.mk
[ -f "$memory" ] || fail "missing $memory"
[ -f "$battery" ] || fail "missing $battery"

for f in "$memory" "$battery"; do
    grep -q 'UNMEASURED' "$f" || fail "$f is not marked unmeasured"
    if grep -E -q 'Pixel|shiba|husky' "$f"; then
        fail "$f names a phone"
    fi
    if grep -E -q 'mAh|benchmark|[0-9]+%|faster than' "$f"; then
        fail "$f states a performance result"
    fi
done

code_of() {
    sed 's/#.*//' "$1"
}

memory_code=$(code_of "$memory")
battery_code=$(code_of "$battery")

printf '%s\n' "$memory_code" | grep -q 'ELE_OS_MEMORY_POLICY := low-ram-aosp-build-flags-unmeasured' \
    || fail 'memory policy marker missing'
printf '%s\n' "$memory_code" | grep -q 'PRODUCT_SYSTEM_SERVER_COMPILER_FILTER := speed-profile' \
    || fail 'system server compiler filter missing'
printf '%s\n' "$memory_code" | grep -q 'PRODUCT_ART_TARGET_INCLUDE_DEBUG_BUILD := false' \
    || fail 'libartd flag missing'
printf '%s\n' "$memory_code" | grep -q 'PRODUCT_MINIMIZE_JAVA_DEBUG_INFO := true' \
    || fail 'Java debug-info flag missing'

printf '%s\n' "$battery_code" | grep -q 'ELE_OS_BATTERY_POLICY := platform-default-unmeasured' \
    || fail 'battery policy marker missing'
# The battery fragment's only assignment is the review marker.
battery_assignments=$(printf '%s\n' "$battery_code" | grep -E ':=' || true)
printf '%s\n' "$battery_assignments" | grep -q 'ELE_OS_BATTERY_POLICY := platform-default-unmeasured' \
    || fail 'battery marker is not an assignment'
extra=$(printf '%s\n' "$battery_assignments" | grep -v 'ELE_OS_BATTERY_POLICY := platform-default-unmeasured' || true)
[ -z "$extra" ] || fail "battery fragment assigns more than the marker: $extra"

for forbidden in \
    'ro\.lmk\.' \
    'dalvik\.vm\.' \
    'pm\.dexopt' \
    'zram' \
    'MALLOC_LOW_MEMORY' \
    'heapgrowthlimit' \
    'heapsize' \
    'upgrade_pressure' \
    'governor' \
    'ro\.config\.low_ram'
do
    if printf '%s\n' "$memory_code" "$battery_code" | grep -E -q "$forbidden"; then
        fail "tuning assignment uses a refused knob ($forbidden)"
    fi
done

# The refusals have to be visible to a reviewer, not only absent.
for phrase in 'pm.dexopt.shared' 'quicken' 'gms_mainline_go' 'zram' 'MALLOC_LOW_MEMORY'; do
    grep -q "$phrase" "$memory" || fail "memory fragment does not mention refusing $phrase"
done
grep -q 'pm.dexopt.shared' "$battery" || fail 'battery fragment does not mention the dexopt tradeoff'
grep -q 'governor' "$battery" || fail 'battery fragment does not mention governors'

if [ -f products/low_ram.mk ]; then
    grep -q 'products/memory_low_ram.mk' products/low_ram.mk \
        || fail 'low-RAM class does not inherit the memory fragment'
fi
if [ -f products/common.mk ]; then
    grep -q 'products/battery.mk' products/common.mk \
        || fail 'shared fragment does not inherit the battery policy'
fi

echo 'host-tuning: ok'
