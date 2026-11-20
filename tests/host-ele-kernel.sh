#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror kernel/native/pressure/test_policy.c -o "$tmp/policy"
"$tmp/policy"
printf 'ELE-Kernel pressure policy checks passed\n'
