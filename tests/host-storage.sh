#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
cxx=${CXX:-c++}
"$cxx" -std=c++17 -Wall -Wextra -Werror -Istorage storage/atomic_store.cpp storage/main.cpp -lcrypto -o "$tmp/store"
mkdir -m 700 "$tmp/private"
dir="$tmp/private"
printf 'first\000record\377' > "$tmp/input"
"$tmp/store" put "$dir" record < "$tmp/input"
"$tmp/store" verify "$dir" record
"$tmp/store" get "$dir" record > "$tmp/output"
cmp "$tmp/input" "$tmp/output"
# Oversized update must keep the previous committed record unchanged.
truncate -s 16777217 "$tmp/large"
if "$tmp/store" put "$dir" record < "$tmp/large"; then exit 1; fi
"$tmp/store" get "$dir" record > "$tmp/output"
cmp "$tmp/input" "$tmp/output"
# Concurrent replacement leaves one whole version.
printf 'other record' > "$tmp/other"
"$tmp/store" put "$dir" record < "$tmp/input" & a=$!
"$tmp/store" put "$dir" record < "$tmp/other" & b=$!
wait "$a"; wait "$b"
"$tmp/store" get "$dir" record > "$tmp/output"
cmp -s "$tmp/input" "$tmp/output" || cmp "$tmp/other" "$tmp/output"
"$tmp/store" put "$dir" empty < /dev/null
"$tmp/store" get "$dir" empty > "$tmp/output"
test ! -s "$tmp/output"
# Tampered content must not be emitted.
printf 'x' | dd of="$dir/record" bs=1 seek=48 conv=notrunc status=none
if "$tmp/store" get "$dir" record > "$tmp/output"; then exit 1; fi
test ! -s "$tmp/output"
ln -s "$tmp/input" "$dir/link"
if "$tmp/store" put "$dir" link < "$tmp/input"; then exit 1; fi
if "$tmp/store" get "$dir" link; then exit 1; fi
mkfifo "$dir/fifo"
if timeout 5 "$tmp/store" get "$dir" fifo; then exit 1; else test "$?" -eq 1; fi
if "$tmp/store" put "$dir" ../bad < "$tmp/input"; then exit 1; fi
chmod 755 "$dir"
if "$tmp/store" put "$dir" bad < "$tmp/input"; then exit 1; fi
test ! -e "$dir/bad"
echo 'Atomic storage and integrity checks passed'
