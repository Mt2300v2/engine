#!/bin/bash
set -e

SRC="main.cpp"
BIN_NAME="build"

mkdir -p build/windows build/linux

g++ "$SRC" -o build/linux/"$BIN_NAME"

x86_64-w64-mingw32-g++ "$SRC" -o build/windows/"$BIN_NAME.exe" -static -static-libgcc -static-libstdc++

echo "Compilación completada."
