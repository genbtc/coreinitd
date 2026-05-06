#!/bin/bash
set -euo pipefail

mkdir -p build
gcc -Isrc -o build/test-loader tests/test-unit-parsing.c src/coreinitd/unit_loader.c
build/test-loader
