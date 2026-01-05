#include "dispatcher_asm.h"

// compiled blocks JMP here instead of RTS. This routine:
// 1. Checks interrupt_check, if set -> RTS to C
// 2. Determines which cache to use based on PC in D3
// 3. Looks up block in appropriate cache, if found -> JMP to it
// 4. Otherwise -> RTS to C to compile the block
// context offsets in jit.h
const unsigned char dispatcher_code[] = {
    0x0c, 0x82, 0x00, 0x00, 0x01, 0xc8,  // 0: cmpi.l #456, d2
    0x64, 0x68,                   // 6: bcc.s -> exit

    // cmpi.w #$4000, d3; bcs.s .bank0
    0x0c, 0x43, 0x40, 0x00,       // 8: cmpi.w #$4000, d3
    0x65, 0x20,                   // 12: bcs.s -> bank0
    // cmpi.w #$8000, d3; bcs.s .banked
    0x0c, 0x43, 0x80, 0x00,       // 14: cmpi.w #$8000, d3
    0x65, 0x30,                   // 18: bcs.s -> banked

    // .upper: (offset 20)
    0x20, 0x6c, 0x00, 0x1c,       // 20: movea.l 28(a4), a0
    0x72, 0x00,                   // 24: moveq #0, d1
    0x32, 0x03,                   // 26: move.w d3, d1
    0x04, 0x41, 0x80, 0x00,       // 28: subi.w #$8000, d1
    0xe5, 0x89,                   // 32: lsl.l #2, d1
    0x20, 0x70, 0x18, 0x00,       // 34: movea.l (a0,d1.l), a0
    0xb0, 0xfc, 0x00, 0x00,       // 38: cmpa.w #0, a0
    0x67, 0x44,                   // 42: beq.s -> exit
    0x4e, 0xd0,                   // 44: jmp (a0)

    // .bank0: (offset 46)
    0x20, 0x6c, 0x00, 0x14,       // 46: movea.l 20(a4), a0
    0x72, 0x00,                   // 50: moveq #0, d1
    0x32, 0x03,                   // 52: move.w d3, d1
    0xe5, 0x89,                   // 54: lsl.l #2, d1
    0x20, 0x70, 0x18, 0x00,       // 56: movea.l (a0,d1.l), a0
    0xb0, 0xfc, 0x00, 0x00,       // 60: cmpa.w #0, a0
    0x67, 0x2e,                   // 64: beq.s -> exit
    0x4e, 0xd0,                   // 66: jmp (a0)

    // .banked: (offset 68)
    0x20, 0x6c, 0x00, 0x18,       // 68: movea.l 24(a4), a0
    0x72, 0x00,                   // 72: moveq #0, d1
    0x12, 0x2c, 0x00, 0x11,       // 74: move.b 17(a4), d1
    0xe5, 0x89,                   // 78: lsl.l #2, d1
    0x20, 0x70, 0x18, 0x00,       // 80: movea.l (a0,d1.l), a0
    0xb0, 0xfc, 0x00, 0x00,       // 84: cmpa.w #0, a0
    0x67, 0x16,                   // 88: beq.s -> exit
    0x72, 0x00,                   // 90: moveq #0, d1
    0x32, 0x03,                   // 92: move.w d3, d1
    0x04, 0x41, 0x40, 0x00,       // 94: subi.w #$4000, d1
    0xe5, 0x89,                   // 98: lsl.l #2, d1
    0x20, 0x70, 0x18, 0x00,       // 100: movea.l (a0,d1.l), a0
    0xb0, 0xfc, 0x00, 0x00,       // 104: cmpa.w #0, a0
    0x67, 0x02,                   // 108: beq.s -> exit
    0x4e, 0xd0,                   // 110: jmp (a0)

    // .exit: (offset 112)
    0x4e, 0x75                    // 112: rts
};
