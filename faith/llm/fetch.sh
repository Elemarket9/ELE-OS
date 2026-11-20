#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
# Fetches llama.cpp and the pinned weight into the user cache. Does not copy either into git.
set -eu
cd "$(dirname "$0")/../.."
pin=faith/llm/PIN

value() {
    awk -F= -v k="$1" '$1==k {print substr($0, index($0,"=")+1); exit}' "$pin"
}

root=$(git rev-parse --show-toplevel)
cache="${XDG_CACHE_HOME:-$HOME/.cache}/ele-os/faith"
case "$cache" in
    "$root"|"$root"/*) echo "refusing to store weights inside the git tree" >&2; exit 1 ;;
esac
mkdir -p "$cache/models"

commit=$(value runtime_commit)
tag=$(value runtime_tag)
src="$cache/llama.cpp"
if [ ! -d "$src/.git" ]; then
    git clone --depth 1 --branch "$tag" https://github.com/ggml-org/llama.cpp.git "$src"
fi
got=$(git -C "$src" rev-parse HEAD)
[ "$got" = "$commit" ] || {
    echo "llama.cpp HEAD $got is not $commit" >&2
    exit 1
}
grep -q 'MIT License' "$src/LICENSE"
grep -q 'The ggml authors' "$src/LICENSE"

# v1.0.0 builds llama-cli only together with the server tools, and that path
# embeds a web UI. llama-completion is the CPU completion program and does not.
if [ ! -x "$src/build/bin/llama-completion" ]; then
    CC="${CC:-gcc}" CXX="${CXX:-g++}" cmake -S "$src" -B "$src/build" \
        -DCMAKE_BUILD_TYPE=Release \
        -DGGML_CUDA=OFF -DGGML_METAL=OFF -DGGML_VULKAN=OFF \
        -DGGML_OPENCL=OFF -DGGML_SYCL=OFF \
        -DLLAMA_CURL=OFF -DLLAMA_BUILD_TESTS=OFF -DLLAMA_BUILD_EXAMPLES=OFF \
        -DLLAMA_BUILD_SERVER=OFF
    cmake --build "$src/build" --target llama-completion -j"$(nproc)"
fi

file=$(value model_weights_file)
dest="$cache/models/$file"
url=$(value model_weights_url)
sum=$(value model_sha256)
bytes=$(value model_bytes)
if [ ! -f "$dest" ]; then
    curl -fL --retry 3 -o "$dest.partial" "$url"
    mv "$dest.partial" "$dest"
fi
got=$(sha256sum "$dest" | awk '{print $1}')
[ "$got" = "$sum" ] || { echo "weight hash mismatch" >&2; exit 1; }
[ "$(wc -c < "$dest" | tr -d ' ')" -eq "$bytes" ] || { echo "weight size mismatch" >&2; exit 1; }
printf 'runtime=%s\nmodel=%s\nbytes=%s\n' "$src/build/bin/llama-completion" "$dest" "$bytes"
