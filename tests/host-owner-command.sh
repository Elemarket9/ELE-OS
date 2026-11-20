#!/bin/sh
# Host-only owner command check. Does not lock, wipe, or locate a handset.
set -eu
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
cxx=${CXX:-c++}
"$cxx" -std=c++17 -Wall -Wextra -Werror -pthread -Iactivation \
    activation/owner_command.cpp tests/owner_command.cpp -lcrypto -o "$tmp/commands"
"$tmp/commands"
