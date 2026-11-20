#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
javac -Xlint:all -Werror -d "$tmp" peripherals/src/org/eleos/peripherals/PeripheralKinds.java tests/PeripheralKindsTest.java
java -cp "$tmp" org.eleos.peripherals.PeripheralKindsTest
