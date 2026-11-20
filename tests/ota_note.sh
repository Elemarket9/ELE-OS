#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
if grep -R -i -n -E 'pixel|shiba|husky|shusky' ota/branch-model.md ota/note.cpp ota/note.h ota/note_main.cpp; then
    echo 'phone-maker names do not belong in the update notes' >&2
    exit 1
fi
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
cxx=${CXX:-c++}
"$cxx" -std=c++17 -Wall -Wextra -Werror -Iota ota/metadata.cpp ota/note.cpp ota/note_main.cpp -lcrypto -o "$tmp/ele-ota-note"

note() {
    "$tmp/ele-ota-note" write "$@"
}

note --target cuttlefish --month 2026-11 --branch-phase maintained \
    --platform-source not-shipped --out "$tmp/idle.txt" >"$tmp/idle.out"
grep -Fxq 'note=valid' "$tmp/idle.out"
grep -Fxq 'built=no' "$tmp/idle.out"
grep -Fxq 'bulletin-is-platform-update=no' "$tmp/idle.out"
grep -Fxq 'firmware=not-claimed' "$tmp/idle.out"
grep -Fxq 'server=none' "$tmp/idle.out"
grep -Fxq 'status=note-written' "$tmp/idle.out"
grep -Fxq 'product=ele_os_cf_x86_64' "$tmp/idle.txt"
grep -Fxq 'platform-source=not-shipped' "$tmp/idle.txt"
grep -Fxq 'aosp-tag=-' "$tmp/idle.txt"
"$tmp/ele-ota-note" check "$tmp/idle.txt" >"$tmp/idle-check.out"
grep -Fxq 'server=none' "$tmp/idle-check.out"

note --target cuttlefish --month 2026-11 --branch-phase maintained \
    --platform-source moved --aosp-tag android-16.0.0_r1 --out "$tmp/moved.txt" >"$tmp/moved.out"
grep -Fxq 'platform-source=moved' "$tmp/moved.out"
grep -Fxq 'aosp-tag=aosp-tag:android-16.0.0_r1' "$tmp/moved.txt"
grep -Fxq 'built=no' "$tmp/moved.txt"

note --target treble-gsi --month 2026-11 --branch-phase maintained \
    --platform-source unchanged --out "$tmp/gsi.txt" >"$tmp/gsi.out"
grep -Fxq 'product=ele_os_gsi_arm64' "$tmp/gsi.txt"
grep -Fxq 'target=treble-gsi' "$tmp/gsi.txt"
grep -Fxq 'vendor-partition=not-in-payload' "$tmp/gsi.txt"
grep -Fxq 'firmware=not-claimed' "$tmp/gsi.txt"

note --target treble-gsi --month 2026-11 --branch-phase extended \
    --platform-source fixes-only --fixes local-fix --out "$tmp/ext.txt" >"$tmp/ext.out"
grep -Fxq 'branch-phase=extended' "$tmp/ext.txt"
grep -Fxq 'platform-source=fixes-only' "$tmp/ext.txt"
grep -Fxq 'fixes=local-fix' "$tmp/ext.txt"
grep -Fxq 'aosp-tag=-' "$tmp/ext.txt"
grep -Fxq 'product=ele_os_gsi_arm64' "$tmp/ext.txt"

if "$tmp/ele-ota-note" write \
    --target treble-gsi \
    --month 2026-11 \
    --branch-phase extended \
    --platform-source moved \
    --aosp-tag android-16.0.0_r1 \
    --fixes local-fix \
    --out "$tmp/bad-ext.txt" 2>"$tmp/bad-ext.err"; then
    echo 'expected an extended note to reject a new platform release' >&2
    exit 1
fi
grep -Fxq 'extended branch does not take a new platform release' "$tmp/bad-ext.err"

if "$tmp/ele-ota-note" write \
    --target cuttlefish \
    --month 2026-11 \
    --branch-phase maintained \
    --platform-source moved \
    --out "$tmp/no-tag.txt" 2>"$tmp/no-tag.err"; then
    echo 'expected a moved note without a tag to fail' >&2
    exit 1
fi
grep -Fxq 'a moved note names an AOSP release tag' "$tmp/no-tag.err"

if "$tmp/ele-ota-note" write \
    --target cuttlefish \
    --month 2026-13 \
    --branch-phase maintained \
    --platform-source not-shipped \
    --out "$tmp/bad-month.txt" 2>"$tmp/bad-month.err"; then
    echo 'expected month 2026-13 to fail' >&2
    exit 1
fi
grep -Fxq 'month must be YYYY-MM' "$tmp/bad-month.err"

if "$tmp/ele-ota-note" write \
    --target cuttlefish \
    --month 2026-11 \
    --branch-phase maintained \
    --platform-source moved \
    --aosp-tag android-latest-release \
    --out "$tmp/branch.txt" 2>"$tmp/branch.err"; then
    echo 'expected a branch name to be refused as a release tag' >&2
    exit 1
fi
grep -Fxq 'a moved note names an AOSP release tag' "$tmp/branch.err"

cp "$tmp/idle.txt" "$tmp/built.txt"
sed -i 's/^built=no$/built=yes/' "$tmp/built.txt"
if "$tmp/ele-ota-note" check "$tmp/built.txt" 2>"$tmp/built.err"; then
    echo 'expected built=yes to fail' >&2
    exit 1
fi
grep -Fxq 'this checker cannot certify a build' "$tmp/built.err"

cp "$tmp/idle.txt" "$tmp/bulletin.txt"
sed -i 's/^bulletin-is-platform-update=no$/bulletin-is-platform-update=yes/' "$tmp/bulletin.txt"
if "$tmp/ele-ota-note" check "$tmp/bulletin.txt" 2>"$tmp/bulletin.err"; then
    echo 'expected a bulletin to be refused as a platform update' >&2
    exit 1
fi
grep -Fxq 'a security bulletin is not a platform update' "$tmp/bulletin.err"

cp "$tmp/gsi.txt" "$tmp/vendor.txt"
sed -i 's/^vendor-partition=not-in-payload$/vendor-partition=virtual-image/' "$tmp/vendor.txt"
if "$tmp/ele-ota-note" check "$tmp/vendor.txt" 2>"$tmp/vendor.err"; then
    echo 'expected a vendor partition claim to fail' >&2
    exit 1
fi
grep -Fxq 'vendor partition stays out of the note' "$tmp/vendor.err"

cp "$tmp/ext.txt" "$tmp/dup.txt"
sed -i 's/^fixes=local-fix$/fixes=local-fix,local-fix/' "$tmp/dup.txt"
if "$tmp/ele-ota-note" check "$tmp/dup.txt" 2>"$tmp/dup.err"; then
    echo 'expected a repeated fix id to fail' >&2
    exit 1
fi
grep -Fxq 'extended note lists fix ids' "$tmp/dup.err"
echo 'OTA note checks passed'
