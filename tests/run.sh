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
sh tests/status.sh
sh tests/ota.sh
"$cxx" -std=c++17 -Wall -Wextra -Werror -Iresource tests/resource_policy.cpp -o "$tmp/resource_policy"
"$tmp/resource_policy"
printf 'Resource advisory policy checks passed\n'
"$cxx" -std=c++17 -Wall -Wextra -Werror -Iresource resource/telemetry.cpp tests/telemetry.cpp -o "$tmp/telemetry"
"$tmp/telemetry"
"$cxx" -std=c++17 -Wall -Wextra -Werror -pthread -Iresource resource/telemetry.cpp resource/main.cpp -o "$tmp/ele-resource"
"$tmp/ele-resource" > "$tmp/resource-report"
[ "$(wc -l < "$tmp/resource-report")" -eq 4 ]
[ "$(grep -c 'thermal=unknown background_allowed=no$' "$tmp/resource-report")" -eq 4 ]
python3 tests/capability_registry.py
