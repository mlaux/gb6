#include "dispatcher_asm.h"

// compiled blocks JMP here instead of RTS. This routine:
// 1. Checks interrupt_check, if set -> RTS to C
// 2. Determines which cache to use based on PC in D0
// 3. Looks up block in appropriate cache, if found -> JMP to it
// 4. Otherwise -> RTS to C to compile the block
//
// Context offsets (a4):
//   16: interrupt_check
//   17: current_rom_bank
//   20: bank0_cache      (0x0000-0x3FFF)
//   24: banked_cache     (0x4000-0x7FFF, indexed by bank)
//   28: upper_cache      (0x8000-0xFFFF)
//
// Assembly:
//   tst.b   16(a4)              ; check interrupt_check
//   bne     .exit
//   cmpi.w  #$4000, d0
//   bcs.s   .bank0              ; PC < $4000
//   cmpi.w  #$8000, d0
//   bcs.s   .banked             ; $4000 <= PC < $8000
//   ; fall through to upper
// .upper:
//   movea.l 28(a4), a0          ; upper_cache
//   moveq   #0, d1
//   move.w  d0, d1
//   subi.w  #$8000, d1          ; index = PC - $8000
//   lsl.l   #2, d1
//   movea.l (a0,d1.l), a0
//   cmpa.w  #0, a0              ; check for NULL
//   beq.s   .exit
//   jmp     (a0)
// .bank0:
//   movea.l 20(a4), a0          ; bank0_cache
//   moveq   #0, d1
//   move.w  d0, d1              ; index = PC
//   lsl.l   #2, d1
//   movea.l (a0,d1.l), a0
//   cmpa.w  #0, a0
//   beq.s   .exit
//   jmp     (a0)
// .banked:
//   movea.l 24(a4), a0          ; banked_cache base
//   moveq   #0, d1
//   move.b  17(a4), d1          ; current_rom_bank
//   lsl.l   #2, d1
//   movea.l (a0,d1.l), a0       ; banked_cache[bank]
//   cmpa.w  #0, a0
//   beq.s   .exit               ; bank not allocated
//   moveq   #0, d1
//   move.w  d0, d1
//   subi.w  #$4000, d1          ; index = PC - $4000
//   lsl.l   #2, d1
//   movea.l (a0,d1.l), a0
//   cmpa.w  #0, a0
//   beq.s   .exit
//   jmp     (a0)
// .exit:
//   rts
const unsigned char dispatcher_code[] = {
    // tst.b 16(a4); bne.s .exit
    0x4a, 0x2c, 0x00, 0x10,       // 0: tst.b 16(a4)
    0x66, 0x68,                   // 4: bne.s -> exit (110-6=104=0x68)
    // cmpi.w #$4000, d0; bcs.s .bank0
    0x0c, 0x40, 0x40, 0x00,       // 6: cmpi.w #$4000, d0
    0x65, 0x20,                   // 10: bcs.s -> bank0 (46-12=34=0x22)
    // cmpi.w #$8000, d0; bcs.s .banked
    0x0c, 0x40, 0x80, 0x00,       // 12: cmpi.w #$8000, d0
    0x65, 0x30,                   // 16: bcs.s -> banked (68-18=50=0x32)

    // .upper: (offset 18)
    0x20, 0x6c, 0x00, 0x1c,       // 18: movea.l 28(a4), a0
    0x72, 0x00,                   // 22: moveq #0, d1
    0x32, 0x00,                   // 24: move.w d0, d1
    0x04, 0x41, 0x80, 0x00,       // 26: subi.w #$8000, d1
    0xe5, 0x89,                   // 30: lsl.l #2, d1
    0x20, 0x70, 0x18, 0x00,       // 32: movea.l (a0,d1.l), a0
    0xb0, 0xfc, 0x00, 0x00,       // 36: cmpa.w #0, a0
    0x67, 0x44,                   // 40: beq.s -> exit (110-42=68=0x44)
    0x4e, 0xd0,                   // 42: jmp (a0)

    // .bank0: (offset 44)
    0x20, 0x6c, 0x00, 0x14,       // 44: movea.l 20(a4), a0
    0x72, 0x00,                   // 48: moveq #0, d1
    0x32, 0x00,                   // 50: move.w d0, d1
    0xe5, 0x89,                   // 52: lsl.l #2, d1
    0x20, 0x70, 0x18, 0x00,       // 54: movea.l (a0,d1.l), a0
    0xb0, 0xfc, 0x00, 0x00,       // 58: cmpa.w #0, a0
    0x67, 0x2e,                   // 62: beq.s -> exit (110-64=46=0x2e)
    0x4e, 0xd0,                   // 64: jmp (a0)

    // .banked: (offset 66)
    0x20, 0x6c, 0x00, 0x18,       // 66: movea.l 24(a4), a0
    0x72, 0x00,                   // 70: moveq #0, d1
    0x12, 0x2c, 0x00, 0x11,       // 72: move.b 17(a4), d1
    0xe5, 0x89,                   // 76: lsl.l #2, d1
    0x20, 0x70, 0x18, 0x00,       // 78: movea.l (a0,d1.l), a0
    0xb0, 0xfc, 0x00, 0x00,       // 82: cmpa.w #0, a0
    0x67, 0x16,                   // 86: beq.s -> exit (110-88=22=0x16)
    0x72, 0x00,                   // 88: moveq #0, d1
    0x32, 0x00,                   // 90: move.w d0, d1
    0x04, 0x41, 0x40, 0x00,       // 92: subi.w #$4000, d1
    0xe5, 0x89,                   // 96: lsl.l #2, d1
    0x20, 0x70, 0x18, 0x00,       // 98: movea.l (a0,d1.l), a0
    0xb0, 0xfc, 0x00, 0x00,       // 102: cmpa.w #0, a0
    0x67, 0x02,                   // 106: beq.s -> exit (110-108=2)
    0x4e, 0xd0,                   // 108: jmp (a0)

    // .exit: (offset 110)
    0x4e, 0x75                    // 110: rts
};
