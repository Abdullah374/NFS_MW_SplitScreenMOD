#!/bin/sh
# Builds NFSMWSplitScreen.asi with MinGW-w64 (32-bit)
set -e
mkdir -p build
i686-w64-mingw32-g++ -O2 -std=c++17 -shared -static -static-libgcc -static-libstdc++ \
    -o build/NFSMWSplitScreen.asi src/main.cpp -lgdi32
cp config/NFSMWSplitScreen.ini build/
echo "Built build/NFSMWSplitScreen.asi"
