#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
python3 -B tests/kernel_build.py
