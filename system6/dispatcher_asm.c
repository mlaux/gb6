#include "cpu_cache.h"
#include "dispatcher_asm.h"
#include "settings.h"

// compiled blocks JMP here instead of RTS. This routine:
// 1. Checks interrupt_check, if set -> RTS to C
// 2. Determines which cache to use based on PC in D3
// 3. Looks up block in appropriate cache, if found -> JMP to it
// 4. Otherwise -> RTS to C to compile the block
// context offsets in jit.h
static unsigned char dispatcher_code[] = {
    0x0c, 0x82, 0x00, 0x00, 0x1c, 0x80,  // 0: cmpi.l #cycles_per_exit, d2
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
static unsigned char patch_helper_code[] = {
    0x22, 0x5f,                   // 0: move.l (sp)+, a1

    // Determine region
    0x0c, 0x43, 0x40, 0x00,       // 2: cmpi.w #$4000, d3
    0x65, 0x2c,                   // 6: bcs.s .bank0 (+44) -> offset 52
    0x0c, 0x43, 0x80, 0x00,       // 8: cmpi.w #$8000, d3
    0x64, 0x36,                   // 12: bcc.s .upper (+54) -> offset 68

    // .banked: (offset 14) - lookup banked_cache[current_bank][d3 - 0x4000]

    // TODO: this scenario can occur, but i think it's saved by the fact that
    // 'jp hl' always goes through the dispatcher, and bank0 code with a
    // hardcoded jp/call $5xxx where the intended target varies by bank is very rare:

    // 1. block A is at address 0x1000 (bank 0, always visible)
    // 2. block A has a patchable exit to address 0x5000 (banked region)
    // 3. with bank 1 active, block A runs, patch_helper finds bank 1's code
    //      at 0x5000, patches block A with JMP.L bank1_code
    // 4. later, bank 2 is switched in
    // 5. some other code jumps to 0x1000 - block A is found in bank0_cache and runs
    // 6. block A's patched JMP goes directly to bank 1's code

    0x20, 0x6c, 0x00, 0x18,       // 14: movea.l 24(a4), a0  [banked_cache]
    0x72, 0x00,                   // 18: moveq #0, d1
    0x12, 0x2c, 0x00, 0x11,       // 20: move.b 17(a4), d1  [current_rom_bank]
    0xe5, 0x89,                   // 24: lsl.l #2, d1
    0x20, 0x70, 0x18, 0x00,       // 26: movea.l (a0,d1.l), a0  [banked_cache[bank]]
    0xb0, 0xfc, 0x00, 0x00,       // 30: cmpa.w #0, a0
    0x67, 0x46,                   // 34: beq.s .no_patch (+70) -> offset 106
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
    0x67, 0x0e,                   // 90: beq.s .no_patch (+14) -> offset 106

    // .do_patch: (offset 92)
    0x43, 0xe9, 0xff, 0xfa,       // 92: lea -6(a1), a1
    0x32, 0xfc, 0x4e, 0xf9,       // 96: move.w #$4ef9, (a1)+  [JMP.L opcode]
    0x22, 0x88,                   // 100: move.l a0, (a1)
    // The trap dispatcher first saves registers D0, D1, D2, A1, and, if bit 8 is 0, A0.
    // The Operating System routine may alter any of the registers D0-D2 and A0-A2,
    // but it must preserve registers D3-D7 and A3-A6. The Operating System routine
    // may return information in register D0 (and A0 if bit 8 is set). To return to
    // the trap dispatcher, the Operating System routine executes the RTS
    // (return from subroutine) instruction.
    // When the trap dispatcher resumes control, first it restores the value of registers
    // D1, D2, A1, A2, and, if bit 8 is 0, A0. The values in registers D0 and,
    // if bit 8 is 1, in A0 are not restored.
    0xa0, 0xbd,                   // 102: FlushCodeCache()
    0x4e, 0xd0,                   // 104: jmp (a0)

    // .no_patch: (offset 106)
    0x4e, 0xd1                    // 106: jmp (a1)
};

unsigned char *get_dispatcher_code(void)
{
    dispatcher_code[2] = (cycles_per_exit >> 24) & 0xff;
    dispatcher_code[3] = (cycles_per_exit >> 16) & 0xff;
    dispatcher_code[4] = (cycles_per_exit >>  8) & 0xff;
    dispatcher_code[5] = (cycles_per_exit      ) & 0xff;

    if (TrapAvailable(_CacheFlush)) {
      // probably ok bc this only happens once before the dispatcher code
      // is executed, but just in case...
      FlushCodeCache();
    }
    return dispatcher_code;
}

unsigned char *get_patch_helper_code(void)
{
    if (!TrapAvailable(_CacheFlush)) {
      // replace _CacheFlush with a nop
      patch_helper_code[102] = 0x4e;
      patch_helper_code[103] = 0x71;
    }
    return patch_helper_code;
}