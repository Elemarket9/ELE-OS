#!/bin/sh
# Copyright 2026 The ELE-OS Authors
# SPDX-License-Identifier: Apache-2.0

# Optional host probe. Not called from tests/run.sh.
# FAITH_PICO2WAVE, FAITH_WHISPER_CLI, and FAITH_WHISPER_MODEL must already exist.
# This script does not download engines or weights.
set -eu
cd "$(dirname "$0")/.."
: "${FAITH_PICO2WAVE:?set FAITH_PICO2WAVE to a pico2wave binary}"
: "${FAITH_WHISPER_CLI:?set FAITH_WHISPER_CLI to a whisper-cli binary}"
: "${FAITH_WHISPER_MODEL:?set FAITH_WHISPER_MODEL to a ggml whisper weight}"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
mkdir -p "$tmp/classes"
find faith/ui/src faith/host/src -name '*.java' ! -name 'FaithVoiceActivity.java' > "$tmp/sources"
javac --release 17 -d "$tmp/classes" @"$tmp/sources" tests/HostEngineProbe.java
java -cp "$tmp/classes" org.eleos.faith.HostEngineProbe \
    "$FAITH_PICO2WAVE" "$FAITH_WHISPER_CLI" "$FAITH_WHISPER_MODEL"
