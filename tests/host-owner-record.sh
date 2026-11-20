#!/bin/sh
# Host-only owner record and reset-surviving lock check. Not a handset test.
set -eu
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
cxx=${CXX:-c++}
"$cxx" -std=c++17 -Wall -Wextra -Werror -pthread -Iactivation \
    activation/owner_proof.cpp activation/reactivation_session.cpp \
    activation/owner_record.cpp activation/theft_lock.cpp \
    tests/owner_record.cpp -lcrypto -o "$tmp/record"
"$tmp/record"
