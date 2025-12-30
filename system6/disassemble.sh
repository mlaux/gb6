#!/bin/sh
../../Retro68-build/toolchain/bin/m68k-apple-macos-objdump -d build/Emulator.code.bin.gdb -j .code00002 > ../objdump.txt
