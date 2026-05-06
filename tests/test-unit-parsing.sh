#!/bin/bash
# Stub test script
gcc -Isrc -o build/test-loader tests/test-unit-parsing.c src/coreinitd/unit_loader.c
build/test-loader
#Loaded example service, UNIX socket, and timer units successfully
