#!/bin/sh

for src_file in *.asm; do
    [ -e "$src_file" ] || continue


    obj_file="$(basename "$src_file" .asm).obj"
    bin_file="$(basename "$src_file" .asm).bin"
    echo "$src_file -> $bin_file"

    rgbasm -o $obj_file $src_file
    rgblink --nopad -o $bin_file $obj_file
done

rm -f *.obj