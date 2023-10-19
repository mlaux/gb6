section "main", rom0

    nop
    ld b, $0
    ld c, $11
    ld d, $22
    ld e, $33
    ld h, $44
    ld l, $55
    ld [hl], $66
    ld a, $77
    ld bc, $0123
    ld de, $4567
    ld hl, $89ab
    ld sp, $cdef
simple_loop:
    dec a
    jr nz, simple_loop

end:
    jr end

db $fd