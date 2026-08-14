#!/bin/sh

set -eu

if [ "$#" -gt 1 ] || { [ "$#" -eq 1 ] && [ "$1" != "--no-test" ]; }; then
    printf '%s\n' "Usage: $0 [--no-test]" >&2
    exit 2
fi

run_tests=1
if [ "$#" -eq 1 ]; then
    run_tests=0
fi

if [ "$run_tests" -eq 1 ]; then
    cmake -S . -B build -DBUILD_TESTING=ON
else
    cmake -S . -B build -DBUILD_TESTING=OFF
fi

cmake --build build -j

if [ "$run_tests" -eq 1 ]; then
    ctest --test-dir build --output-on-failure
fi

printf '%s\n' "tux-dock successfully compiled!"
