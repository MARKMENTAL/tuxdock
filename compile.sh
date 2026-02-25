#!/bin/sh

cmake -S . -B build && cmake --build build -j && echo "tux-dock successfully compiled!"
