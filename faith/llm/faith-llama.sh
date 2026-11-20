#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
# Reads a prompt on stdin and writes llama-completion's stdout.
# Does not perform actions. CPU only. A missing weight is an error, not a made-up answer.
set -eu
cd "$(dirname "$0")/../.."
pin=faith/llm/PIN

value() {
    awk -F= -v k="$1" '$1==k {print substr($0, index($0,"=")+1); exit}' "$pin"
}

cache="${XDG_CACHE_HOME:-$HOME/.cache}/ele-os/faith"
model="$cache/models/$(value model_weights_file)"
bin="$cache/llama.cpp/build/bin/llama-completion"
[ -f "$model" ] || { echo "pinned weight is not fetched; run faith/llm/fetch.sh" >&2; exit 2; }
[ -x "$bin" ] || { echo "llama-completion is not built; run faith/llm/fetch.sh" >&2; exit 2; }

prompt=$(cat)
[ -n "$prompt" ] || { echo "empty prompt" >&2; exit 2; }

if [ -n "${FAITH_MEMORY_BUDGET_BYTES:-}" ]; then
    size=$(wc -c < "$model" | tr -d ' ')
    if [ "$size" -gt "$FAITH_MEMORY_BUDGET_BYTES" ]; then
        echo "weight file exceeds the memory budget; not loading" >&2
        exit 1
    fi
fi
echo "faith-llama: prototype host runtime. Not a Cuttlefish boot and not a GSI boot. Resident set is not a phone measurement." >&2

# The prompt is an argument to the pinned binary, not a shell command.
exec "$bin" \
    -m "$model" \
    --offline \
    -ngl 0 \
    --fit off \
    -c 512 \
    -n 32 \
    --temp 0 \
    --single-turn \
    --no-display-prompt \
    --simple-io \
    --color off \
    -p "$prompt"
