#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
cxx=${CXX:-c++}
"$cxx" -std=c++17 -Wall -Wextra -Werror -Iactivation activation/owner_proof.cpp \
    tests/owner_proof.cpp -lcrypto -o "$tmp/proof"
"$tmp/proof"
"$cxx" -std=c++17 -Wall -Wextra -Werror -pthread -Iactivation activation/*.cpp \
    tests/reactivation.cpp -lcrypto -o "$tmp/session"
mkdir "$tmp/fixture"
"$tmp/session" "$tmp/fixture"
"$cxx" -std=c++17 -Wall -Wextra -Werror -pthread -Iactivation \
    activation/owner_proof.cpp activation/reactivation_session.cpp \
    activation/owner_record.cpp activation/theft_lock.cpp \
    tests/owner_record.cpp -lcrypto -o "$tmp/record"
"$tmp/record"
sh tests/status.sh
