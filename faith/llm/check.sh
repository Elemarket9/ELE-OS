#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
# Checks the pin. Does not download, build, or run a model.
set -eu
cd "$(dirname "$0")/../.."
pin=faith/llm/PIN

fail() {
    printf 'faith-llm-pin: %s\n' "$1" >&2
    exit 1
}

value() {
    awk -F= -v k="$1" '$1==k {print substr($0, index($0,"=")+1); exit}' "$pin"
}

need() {
    got=$(value "$1")
    [ -n "$got" ] || fail "missing $1"
    printf '%s' "$got"
}

[ "$(need runtime_name)" = "llama.cpp" ] || fail "runtime must be llama.cpp"
[ "$(need runtime_license)" = "MIT" ] || fail "runtime license must be MIT"
[ "$(need runtime_tag)" = "v1.0.0" ] || fail "runtime tag drifted"
commit=$(need runtime_commit)
printf '%s' "$commit" | grep -Eq '^[0-9a-f]{40}$' || fail "runtime commit is not 40 hex digits"
[ "$(need model_name)" = "SmolLM2-135M-Instruct" ] || fail "model name drifted"
[ "$(need model_license)" = "Apache-2.0" ] || fail "model license must be Apache-2.0"
[ "$(need model_publisher)" = "HuggingFaceTB" ] || fail "model publisher drifted"
bytes=$(need model_bytes)
[ "$bytes" -eq 105454144 ] || fail "model_bytes must stay the pinned size"
sum=$(need model_sha256)
printf '%s' "$sum" | grep -Eq '^[0-9a-f]{64}$' || fail "model sha256 is not 64 hex digits"
url=$(need model_weights_url)
printf '%s' "$url" | grep -q '^https://' || fail "weight URL must be https"
printf '%s' "$(need runtime_upstream)" | grep -q '^https://' || fail "runtime URL must be https"
printf '%s' "$(need model_license_url)" | grep -q '^https://' || fail "license URL must be https"

if find . -path './.git' -prune -o -name '*.gguf' -print | grep -q .; then
    fail "weight files must not be committed"
fi
if grep -R -n -i -E 'pixel|shiba' --exclude=check.sh faith/llm; then
    fail "the runtime pin must not name a phone brand or a device codename"
fi

cache="${XDG_CACHE_HOME:-$HOME/.cache}/ele-os/faith/models/$(need model_weights_file)"
if [ -f "$cache" ]; then
    got=$(sha256sum "$cache" | awk '{print $1}')
    [ "$got" = "$sum" ] || fail "cached weight hash does not match the pin"
    [ "$(wc -c < "$cache" | tr -d ' ')" -eq "$bytes" ] || fail "cached weight size does not match the pin"
fi

printf 'Faith model pin checks passed (no download and no inference)\n'
