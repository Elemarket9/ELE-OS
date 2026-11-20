#!/bin/sh
# Copyright 2026 The ELE-OS Authors
# SPDX-License-Identifier: Apache-2.0

# Policy checks only. They do not run Pico, whisper.cpp, or a microphone.
set -eu
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
mkdir -p "$tmp/classes"
# FaithVoiceActivity needs the Android SDK and is not part of this host run.
find faith/ui/src -name '*.java' ! -name 'FaithVoiceActivity.java' > "$tmp/sources"
javac --release 17 -d "$tmp/classes" @"$tmp/sources" tests/FaithVoiceTest.java
java -cp "$tmp/classes" org.eleos.faith.FaithVoiceTest
