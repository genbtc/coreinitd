#!/bin/bash
# Stub test script
gcc -Isrc -o test-loader tests/test-unit-parsing.c src/coreinitd/unit_loader.c
./test-loader
