#!/system/bin/sh
# A local snapshot for comparing ELE-OS builds. Never infer a secure device
# from a missing boot property, and keep advertised patch dates distinct.
set -eu

property() {
    value=$(getprop "$2") || return 1
    printf '%s=%s\n' "$1" "${value:-unknown}"
}

property product ro.product.name
property build_fingerprint ro.build.fingerprint
property system_patch ro.build.version.security_patch
property vendor_patch ro.vendor.build.security_patch
property verified_boot_state ro.boot.verifiedbootstate
property bootloader_locked ro.boot.flash.locked
awk '
    $1 == "MemTotal:" && $2 ~ /^[0-9]+$/ && $3 == "kB" { total = $2 }
    $1 == "MemAvailable:" && $2 ~ /^[0-9]+$/ && $3 == "kB" { available = $2 }
    END {
        if (total == "" || available == "") exit 1
        printf "memory_total_kib=%s\nmemory_available_kib=%s\n", total, available
    }
' /proc/meminfo
