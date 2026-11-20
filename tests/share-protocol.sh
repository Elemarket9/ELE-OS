#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

if ! command -v javac >/dev/null 2>&1 || ! command -v java >/dev/null 2>&1; then
    echo "share protocol tests require a JDK 17 or newer" >&2
    exit 1
fi

mkdir -p "$tmp/classes"
javac --release 17 -Werror -d "$tmp/classes" \
    "$root/share/protocol/TransferProtocol.java" \
    "$root/tests/share/TransferProtocolTest.java"
java -cp "$tmp/classes" org.eleos.share.protocol.TransferProtocolTest
