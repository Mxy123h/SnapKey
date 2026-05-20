#!/bin/bash

set -e

# MSYS2 toolchain path
if [ -d "/ucrt64/bin" ]; then
    export PATH="/ucrt64/bin:/usr/bin:$PATH"
elif [ -d "/mingw64/bin" ]; then
    export PATH="/mingw64/bin:/usr/bin:$PATH"
else
    export PATH="/usr/bin:$PATH"
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# resource file
windres -I "$PROJECT_ROOT" -o "$SCRIPT_DIR/resources.o" "$PROJECT_ROOT/resources.rc"

# compile src
g++ -o "$PROJECT_ROOT/SnapKey.exe" "$PROJECT_ROOT/SnapKey.cpp" "$SCRIPT_DIR/resources.o" -mwindows -std=c++17 -finput-charset=UTF-8 -fexec-charset=UTF-8 -static

# success yes/no
if [ $? -eq 0 ]; then
    echo "Compilation successful: SnapKey.exe created in project root"
else
    echo "Compilation failed."
fi
