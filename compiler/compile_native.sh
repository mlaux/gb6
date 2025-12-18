#!/bin/sh
mkdir -p build_native
cd build_native
cmake .. -DCMAKE_TOOLCHAIN_FILE=../../../Retro68-build/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake
make
cd ..
