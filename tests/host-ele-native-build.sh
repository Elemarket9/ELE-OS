#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
python3 -B tests/ele_native_build.py
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
"${CC:-cc}" -static -O2 -Wall -Wextra -Werror kernel/native/smoke_init.c -o "$tmp/init"
