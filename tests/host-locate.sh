#!/bin/sh
# Host-only locate seal check. Does not boot Cuttlefish or contact an endpoint.
set -eu
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
cxx=${CXX:-c++}
"$cxx" -std=c++17 -Wall -Wextra -Werror -Iactivation \
    activation/locate_payload.cpp tests/locate_payload.cpp -lcrypto -o "$tmp/locate"
"$tmp/locate"
