#include "dispatcher_asm.h"

// compiled blocks JMP here instead of RTS. This routine:
// 1. Checks interrupt_check, if set -> RTS to C
// 2. Determines which cache to use based on PC in D3
// 3. Looks up block in appropriate cache, if found -> JMP to it
// 4. Otherwise -> RTS to C to compile the block
// context offsets in jit.h
const unsigned char dispatcher_code[] = {
    0x0c, 0x82, 0x00, 0x01, 0x12, 0x50,  // 0: cmpi.l #456, d2
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

// patch_helper: called via JSR from patchable block exits
// On entry: return address on stack points to after the JSR (the exit: rts)
// D3 = target GB PC
// A4 = context pointer
//
// This routine:
// 1. Checks if target is in banked region (skip patching if so)
// 2. Looks up target in cache
// 3. If found: patches the JSR into JMP.L and jumps to target
// 4. If not found: jumps to exit which RTSs to C
const unsigned char patch_helper_code[] = {
    0x22, 0x5f,                   // 0: move.l (sp)+, a1

    // Determine region
    0x0c, 0x43, 0x40, 0x00,       // 2: cmpi.w #$4000, d3
    0x65, 0x2c,                   // 6: bcs.s .bank0 (+44) -> offset 52
    0x0c, 0x43, 0x80, 0x00,       // 8: cmpi.w #$8000, d3
    0x64, 0x36,                   // 12: bcc.s .upper (+54) -> offset 68

    // .banked: (offset 14) - lookup banked_cache[current_bank][d3 - 0x4000]
    // TODO this is incorrect for actual banked code, just temp for testing:
    // - block A in bank 1 jumps to address X
    // - A is patched to JMP.L to block B (compiled when bank 1 was active)
    // - later, bank 2 is active, and it jumps to address X
    // - the patched JMP.L still goes to block B (bank 1's code), which is wrong
    0x20, 0x6c, 0x00, 0x18,       // 14: movea.l 24(a4), a0  [banked_cache]
    0x72, 0x00,                   // 18: moveq #0, d1
    0x12, 0x2c, 0x00, 0x11,       // 20: move.b 17(a4), d1  [current_rom_bank]
    0xe5, 0x89,                   // 24: lsl.l #2, d1
    0x20, 0x70, 0x18, 0x00,       // 26: movea.l (a0,d1.l), a0  [banked_cache[bank]]
    0xb0, 0xfc, 0x00, 0x00,       // 30: cmpa.w #0, a0
    0x67, 0x48,                   // 34: beq.s .no_patch (+72) -> offset 108
    0x72, 0x00,                   // 36: moveq #0, d1
    0x32, 0x03,                   // 38: move.w d3, d1
    0x04, 0x41, 0x40, 0x00,       // 40: subi.w #$4000, d1
    0xe5, 0x89,                   // 44: lsl.l #2, d1
    0x20, 0x70, 0x18, 0x00,       // 46: movea.l (a0,d1.l), a0
    0x60, 0x22,                   // 50: bra.s .check_found (+34) -> offset 86

    // .bank0: (offset 52)
    0x20, 0x6c, 0x00, 0x14,       // 52: movea.l 20(a4), a0  [bank0_cache]
    0x72, 0x00,                   // 56: moveq #0, d1
    0x32, 0x03,                   // 58: move.w d3, d1
    0xe5, 0x89,                   // 60: lsl.l #2, d1
    0x20, 0x70, 0x18, 0x00,       // 62: movea.l (a0,d1.l), a0
    0x60, 0x12,                   // 66: bra.s .check_found (+18) -> offset 86

    // .upper: (offset 68)
    0x20, 0x6c, 0x00, 0x1c,       // 68: movea.l 28(a4), a0  [upper_cache]
    0x72, 0x00,                   // 72: moveq #0, d1
    0x32, 0x03,                   // 74: move.w d3, d1
    0x04, 0x41, 0x80, 0x00,       // 76: subi.w #$8000, d1
    0xe5, 0x89,                   // 80: lsl.l #2, d1
    0x20, 0x70, 0x18, 0x00,       // 82: movea.l (a0,d1.l), a0

    // .check_found: (offset 86)
    0xb0, 0xfc, 0x00, 0x00,       // 86: cmpa.w #0, a0
    0x67, 0x10,                   // 90: beq.s .no_patch (+16) -> offset 108

    // .do_patch: (offset 92)
    0x52, 0xac, 0x00, 0x34,       // 92: addq.l #1, 52(a4)  [increment patch_count]
    0x43, 0xe9, 0xff, 0xfa,       // 96: lea -6(a1), a1
    0x32, 0xfc, 0x4e, 0xf9,       // 100: move.w #$4ef9, (a1)+  [JMP.L opcode]
    0x22, 0x88,                   // 104: move.l a0, (a1)
    0x4e, 0xd0,                   // 106: jmp (a0)

    // .no_patch: (offset 108)
    0x4e, 0xd1                    // 108: jmp (a1)
};
