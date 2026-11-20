#!/bin/sh
# Reject Google, Play, and microG names if this tree adds them.
# No image is installed and no package list from a device is read.
# SPDX-License-Identifier: Apache-2.0
set -eu
cd "$(dirname "$0")/.."

fail() {
    printf 'host-debloat: %s\n' "$1" >&2
    exit 1
}

list=products/no_google_packages.txt
[ -f "$list" ] || fail "missing $list"
[ -f products/debloat.mk ] || fail 'missing products/debloat.mk'

grep -q 'no_google_packages.txt' products/debloat.mk || fail 'debloat fragment does not name the list'
grep -q 'no-google-services' products/debloat.mk || fail 'debloat fragment lost its policy marker'
grep -q 'UNMEASURED' products/debloat.mk || fail 'debloat fragment is not marked unmeasured'
if grep -E '^[^#]*filter-out' products/debloat.mk; then
    fail 'debloat fragment pretends to filter PRODUCT_PACKAGES'
fi

for item in \
    'module GmsCore' \
    'module Phonesky' \
    'module GoogleServicesFramework' \
    'module Velvet' \
    'module FakeStore' \
    'module talkback' \
    'module AndroidAccessibilitySuite' \
    'package com.google.android.gms' \
    'package com.android.vending' \
    'package com.google.android.gsf' \
    'package org.microg.gms' \
    'package com.google.android.googlequicksearchbox'
do
    grep -Fxq "$item" "$list" || fail "list missing: $item"
done

if grep -Fxq 'package com.google.android.marvin.talkback' "$list"; then
    fail 'TalkBack source package must stay off the forbidden list'
fi
if grep -Fxq 'package com.android.webview' "$list"; then
    fail 'AOSP WebView must stay off the forbidden list'
fi
grep -q 'google/talkback' "$list" || fail 'list does not record the TalkBack source exception'

# Every entry is "module name" or "package name", with no duplicates.
awk '
    /^#/ || NF == 0 { next }
    NF != 2 || ($1 != "module" && $1 != "package") {
        printf "bad line: %s\n", $0 > "/dev/stderr"
        exit 1
    }
    {
        key = $1 " " $2
        if (seen[key]++) {
            printf "duplicate: %s\n", key > "/dev/stderr"
            exit 1
        }
        print $2
    }
' "$list" >"${TMPDIR:-/tmp}/ele-os-debloat-names"

if [ -f products/common.mk ]; then
    grep -q 'products/debloat.mk' products/common.mk || fail 'common fragment does not inherit debloat.mk'
fi

# Whole token only, so a short module name such as Maps does not match
# inside an unrelated identifier. The list file is not a source file.
# check-build-contract.sh names the same modules in its reject pattern.
hits=$(mktemp)
names=${TMPDIR:-/tmp}/ele-os-debloat-names
trap 'rm -f "$hits" "$names"' EXIT
pattern=$(sed 's/[].[*^$()+?{|\\]/\\&/g' "$names" | awk 'NR > 1 { printf "|" } { printf "%s", $0 }')
find . -type f \
    \( -name '*.mk' -o -name '*.bp' -o -name '*.xml' -o -name '*.java' \
       -o -name '*.sh' -o -name '*.cpp' -o -name '*.h' \) \
    ! -path './products/debloat.mk' \
    ! -path './tests/host-debloat.sh' \
    ! -path './tests/check-build-contract.sh' \
    -print0 |
    xargs -0 grep -n -E "(^|[^A-Za-z0-9_])(${pattern})([^A-Za-z0-9_]|$)" >"$hits" || true

if [ -s "$hits" ]; then
    printf 'host-debloat: forbidden name found\n' >&2
    cat "$hits" >&2
    exit 1
fi

echo 'host-debloat: ok'
