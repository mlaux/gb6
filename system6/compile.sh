#!/bin/sh
mkdir -p build

# inject git sha into emulator.h
GIT_SHA=$(git rev-parse --short HEAD)
sed -i.bak "s/\${GIT_SHA}/$GIT_SHA/" emulator.h

cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../../Retro68-build/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake
make
cd ..

# restore emulator.h
mv emulator.h.bak emulator.h
