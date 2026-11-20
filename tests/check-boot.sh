#!/bin/sh
# Read-only evidence from a single explicitly selected emulator.
set -eu
serial=${1:?usage: sh tests/check-boot.sh SERIAL}
timeout 180 adb -s "$serial" wait-for-device
timeout 180 adb -s "$serial" shell 'until [ "$(getprop sys.boot_completed)" = 1 ]; do sleep 1; done'
raw=$(timeout 20 adb -s "$serial" shell /product/bin/ele-status)
report=$(printf '%s' "$raw" | tr -d '\r')
printf '%s\n' "$report"
printf '%s\n' "$report" | grep -Fxq 'product=ele_os_cf_x86_64'
printf '%s\n' "$report" | grep -Eq '^memory_total_kib=[1-9][0-9]*$'
