#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
cat > "$tmp/getprop" <<'MOCK'
#!/bin/sh
case "$1" in
    ro.product.name) echo ele_os_cf_x86_64 ;;
    ro.boot.verifiedbootstate) echo orange ;;
    ro.boot.flash.locked) echo 0 ;;
esac
MOCK
chmod +x "$tmp/getprop"
export PATH="$tmp:$PATH"
sh status/ele-status.sh > "$tmp/report"
for line in product=ele_os_cf_x86_64 verified_boot_state=orange bootloader_locked=0 system_patch=unknown vendor_patch=unknown; do
    grep -Fxq "$line" "$tmp/report"
done
grep -Eq '^memory_available_kib=[0-9]+$' "$tmp/report"
grep -Eq '^memory_total_kib=[1-9][0-9]*$' "$tmp/report"
printf '#!/bin/sh\nexit 1\n' > "$tmp/getprop"
if sh status/ele-status.sh > /dev/null; then
    echo 'Expected failure when getprop is unavailable' >&2
    exit 1
fi
echo 'Host snapshot checks passed'
