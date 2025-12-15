#include <stdio.h>
#include <stdlib.h>

#include "compiler.h"
#include "emitters.h"

void compiler_init(void)
{
    // nothing for now
}

// Set Z flag in D7 based on current 68k CCR, preserving other flags
static void compile_set_z_flag(struct code_block *block)
{
    // seq D5 (D5 = 0xff if Z, 0x00 if NZ)
    emit_scc(block, 0x7, REG_68K_D_SCRATCH_1);
    // scratch = 0x80 if Z was set
    emit_andi_b_dn(block, REG_68K_D_SCRATCH_1, 0x80);
    // clear Z bit in D7
    emit_andi_b_dn(block, REG_68K_D_FLAGS, 0x7f);
    // D7 |= new Z bit
    emit_or_b_dn_dn(block, REG_68K_D_SCRATCH_1, REG_68K_D_FLAGS);
}

// Set Z and C flags in D7 based on current 68k CCR, also set N=1 (for sub/cp)
static void compile_set_znc_flags(struct code_block *block)
{
    // extract both flags first using scc (scc doesn't affect CCR)
    // seq D5 (D5 = 0xff if Z, 0x00 if NZ)
    emit_scc(block, 0x7, REG_68K_D_SCRATCH_1);
    // scs D7 (D7 = 0xff if C, 0x00 if NC)
    emit_scc(block, 0x5, REG_68K_D_FLAGS);

    // now CCR doesn't matter anymore
    emit_andi_b_dn(block, REG_68K_D_SCRATCH_1, 0x80); // D5 = 0x80 if Z was set
    emit_andi_b_dn(block, REG_68K_D_FLAGS, 0x10); // D7 = 0x10 if C was set
    emit_or_b_dn_dn(block, REG_68K_D_SCRATCH_1, REG_68K_D_FLAGS);   // D7 = Z_bit | C_bit

    // Add N=1 flag for subtract operations
    emit_ori_b_dn(block, REG_68K_D_FLAGS, 0x40);  // D7 |= 0x40 (N flag)
}

// Reconstruct BC from split format (0x00BB00CC) into D1.w as 0xBBCC
static void compile_bc_to_addr(struct code_block *block)
{
    emit_move_l_dn_dn(block, REG_68K_D_BC, REG_68K_D_SCRATCH_1);  // D1 = 0x00BB00CC
    emit_move_w_dn_dn(block, REG_68K_D_SCRATCH_1, REG_68K_D_SCRATCH_2);  // D2.w = 0x00CC
    emit_swap(block, REG_68K_D_SCRATCH_1);  // D1 = 0x00CC00BB
    emit_lsl_w_imm_dn(block, 8, REG_68K_D_SCRATCH_1);  // D1.w = 0xBB00
    emit_or_b_dn_dn(block, REG_68K_D_SCRATCH_2, REG_68K_D_SCRATCH_1);  // D1.w = 0xBBCC
}

// Reconstruct DE from split format (0x00DD00EE) into D1.w as 0xDDEE
static void compile_de_to_addr(struct code_block *block)
{
    emit_move_l_dn_dn(block, REG_68K_D_DE, REG_68K_D_SCRATCH_1);  // D1 = 0x00DD00EE
    emit_move_w_dn_dn(block, REG_68K_D_SCRATCH_1, REG_68K_D_SCRATCH_2);  // D2.w = 0x00EE
    emit_swap(block, REG_68K_D_SCRATCH_1);  // D1 = 0x00EE00DD
    emit_lsl_w_imm_dn(block, 8, REG_68K_D_SCRATCH_1);  // D1.w = 0xDD00
    emit_or_b_dn_dn(block, REG_68K_D_SCRATCH_2, REG_68K_D_SCRATCH_1);  // D1.w = 0xDDEE
}

// Call dmg_write(dmg, addr, val) - addr in D1, val in D4 (A register)
static void compile_call_dmg_write(struct code_block *block)
{
    // Push args right-to-left: val, addr, dmg
    emit_push_w_dn(block, REG_68K_D_A);  // push value (A register = D4)
    emit_push_w_dn(block, REG_68K_D_SCRATCH_1);  // push address (D1)
    emit_push_l_disp_an(block, JIT_CTX_DMG, REG_68K_A_CTX);  // push dmg pointer
    emit_movea_l_disp_an_an(block, JIT_CTX_WRITE, REG_68K_A_CTX, REG_68K_A_SCRATCH_1);  // A0 = write func
    emit_jsr_ind_an(block, REG_68K_A_SCRATCH_1);  // call dmg_write
    emit_addq_l_an(block, 7, 8);  // clean up stack (4 + 2 + 2 = 8 bytes)
}

// Call dmg_read(dmg, addr) - addr in D1, result goes to D4 (A register)
static void compile_call_dmg_read(struct code_block *block)
{
    // Push args right-to-left: addr, dmg
    emit_push_w_dn(block, REG_68K_D_SCRATCH_1);  // push address (D1)
    emit_push_l_disp_an(block, JIT_CTX_DMG, REG_68K_A_CTX);  // push dmg pointer
    emit_movea_l_disp_an_an(block, JIT_CTX_READ, REG_68K_A_CTX, REG_68K_A_SCRATCH_1);  // A0 = read func
    emit_jsr_ind_an(block, REG_68K_A_SCRATCH_1);  // call dmg_read
    emit_addq_l_an(block, 7, 6);  // clean up stack (4 + 2 = 6 bytes)

    // Move result from D0 to D4 (A register)
    emit_move_b_dn_dn(block, REG_68K_D_NEXT_PC, REG_68K_D_A);
}

static void compile_ld_imm16_split(
    struct code_block *block,
    uint8_t reg,
    uint8_t *gb_code,
    uint16_t *src_ptr
) {
    uint8_t lobyte = gb_code[*src_ptr],
            hibyte = gb_code[*src_ptr + 1];
    *src_ptr += 2;
    emit_move_l_dn(block, reg, hibyte << 16);
    emit_move_w_dn(block, reg, lobyte);
}

static void compile_ld_imm16_contiguous(
    struct code_block *block,
    uint8_t reg,
    uint8_t *gb_code,
    uint16_t *src_ptr
) {
    uint8_t lobyte = gb_code[*src_ptr],
            hibyte = gb_code[*src_ptr + 1];
    *src_ptr += 2;
    emit_movea_w_imm16(block, reg, hibyte << 8 | lobyte);
}

// returns 1 if jr ended the block, 0 if it's a backward jump within block
static int compile_jr(
    struct code_block *block,
    uint8_t *gb_code,
    uint16_t *src_ptr,
    uint16_t src_address
) {
    int8_t disp;
    int16_t target_gb_offset;
    uint16_t target_m68k, target_gb_pc;
    int16_t m68k_disp;

    disp = (int8_t) gb_code[*src_ptr];
    (*src_ptr)++;

    // jr displacement is relative to PC after the jr instruction
    // *src_ptr now points to the byte after jr, so target = *src_ptr + disp
    target_gb_offset = (int16_t) *src_ptr + disp;

    // Check if this is a backward jump to a location we've already compiled
    if (target_gb_offset >= 0 && target_gb_offset < (int16_t) (*src_ptr - 2)) {
        // Backward jump within block - emit bra.w
        target_m68k = block->m68k_offsets[target_gb_offset];
        // bra.w displacement is relative to PC after the opcode word (before extension)
        // current position = block->length, PC after opcode = block->length + 2
        m68k_disp = (int16_t) target_m68k - (int16_t) (block->length + 2);
        emit_bra_w(block, m68k_disp);
        return 0;
    }

    // Forward jump or outside block - go through dispatcher
    target_gb_pc = src_address + target_gb_offset;
    emit_moveq_dn(block, REG_68K_D_NEXT_PC, 0);
    emit_move_w_dn(block, REG_68K_D_NEXT_PC, target_gb_pc);
    emit_rts(block);
    return 1;
}

// Compile conditional relative jump (jr nz, jr z, jr nc, jr c)
// flag_bit: which bit in D7 to test (7=Z, 4=C)
// branch_if_set: if true, branch when flag is set; if false, branch when clear
static void compile_jr_cond(
    struct code_block *block,
    uint8_t *gb_code,
    uint16_t *src_ptr,
    uint16_t src_address,
    uint8_t flag_bit,
    int branch_if_set
) {
    int8_t disp;
    int16_t target_gb_offset;
    uint16_t target_m68k, target_gb_pc;
    int16_t m68k_disp;

    disp = (int8_t) gb_code[*src_ptr];
    (*src_ptr)++;

    target_gb_offset = (int16_t) *src_ptr + disp;

    // Test the flag bit in D7
    // btst sets 68k Z=1 if tested bit is 0, Z=0 if tested bit is 1
    emit_btst_imm_dn(block, flag_bit, 7);

    // Check if this is a backward jump within block
    if (target_gb_offset >= 0 && target_gb_offset < (int16_t) (*src_ptr - 2)) {
        // Backward jump - emit conditional branch
        target_m68k = block->m68k_offsets[target_gb_offset];
        m68k_disp = (int16_t) target_m68k - (int16_t) (block->length + 2);

        if (branch_if_set) {
            // Branch if flag is set: btst gives Z=0 when bit=1, so use bne
            emit_bne_w(block, m68k_disp);
        } else {
            // Branch if flag is clear: btst gives Z=1 when bit=0, so use beq
            emit_beq_w(block, m68k_disp);
        }
        return; // block continues (fall-through path)
    }

    // Forward/external jump - conditionally exit to dispatcher
    // If condition NOT met, skip the exit sequence
    target_gb_pc = src_address + target_gb_offset;

    if (branch_if_set) {
        // Skip exit if flag is clear (btst Z=1 when bit=0)
        emit_beq_w(block, 10);  // skip: moveq(2) + move.w(4) + rts(2) + ext(2) = 10
    } else {
        // Skip exit if flag is set (btst Z=0 when bit=1)
        emit_bne_w(block, 10);
    }

    emit_moveq_dn(block, REG_68K_D_NEXT_PC, 0);
    emit_move_w_dn(block, REG_68K_D_NEXT_PC, target_gb_pc);
    emit_rts(block);
}

struct code_block *compile_block(uint16_t src_address, uint8_t *gb_code)
{
    struct code_block *block;
    uint16_t src_ptr = 0;
    uint8_t op;
    int done = 0;

    block = malloc(sizeof *block);
    if (!block) {
        return NULL;
    }

    block->length = 0;
    block->src_address = src_address;

    while (!done) {
        block->m68k_offsets[src_ptr] = block->length;
        op = gb_code[src_ptr++];

        switch (op) {
        case 0x00: // nop
            // emit nothing
            break;

        case 0x02: // ld (bc), a
            compile_bc_to_addr(block);  // BC -> D5.w
            compile_call_dmg_write(block);  // dmg_write(dmg, D5.w, A)
            break;

        case 0x0a: // ld a, (bc)
            compile_bc_to_addr(block);  // BC -> D5.w
            compile_call_dmg_read(block);  // A = dmg_read(dmg, D5.w)
            break;

        case 0x12: // ld (de), a
            compile_de_to_addr(block);  // DE -> D5.w
            compile_call_dmg_write(block);  // dmg_write(dmg, D5.w, A)
            break;

        case 0x1a: // ld a, (de)
            compile_de_to_addr(block);  // DE -> D5.w
            compile_call_dmg_read(block);  // A = dmg_read(dmg, D5.w)
            break;

        case 0x01: // ld bc, imm16
            compile_ld_imm16_split(block, REG_68K_D_BC, gb_code, &src_ptr);
            break;
        case 0x11: // ld de, imm16
            compile_ld_imm16_split(block, REG_68K_D_DE, gb_code, &src_ptr);
            break;
        case 0x21: // ld hl, imm16
            compile_ld_imm16_contiguous(block, REG_68K_A_HL, gb_code, &src_ptr);
            break;
        case 0x31: // ld sp, imm16
            compile_ld_imm16_contiguous(block, REG_68K_A_SP, gb_code, &src_ptr);
            break;

        case 0x06: // ld b, imm8
            emit_swap(block, REG_68K_D_BC);
            emit_move_b_dn(block, REG_68K_D_BC, gb_code[src_ptr++]);
            emit_swap(block, REG_68K_D_BC);
            break;

        case 0x0e: // ld c, imm8
            emit_move_b_dn(block, REG_68K_D_BC, gb_code[src_ptr++]);
            break;

        case 0x16: // ld d, imm8
            emit_swap(block, REG_68K_D_DE);
            emit_move_b_dn(block, REG_68K_D_DE, gb_code[src_ptr++]);
            emit_swap(block, REG_68K_D_DE);
            break;

        case 0x1e: // ld e, imm8
            emit_move_b_dn(block, REG_68K_D_DE, gb_code[src_ptr++]);
            break;

        case 0x26: // ld h, imm8
            // move.w a0, d5
            emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);
             // rol.w #8, d5
            emit_rol_w_8(block, REG_68K_D_SCRATCH_1);
            emit_move_b_dn(block, REG_68K_D_SCRATCH_1, gb_code[src_ptr++]);
            // ror.w #8, d5
            emit_ror_w_8(block, REG_68K_D_SCRATCH_1);
            // movea.w d5, a0
            emit_movea_w_dn_an(block, REG_68K_D_SCRATCH_1, REG_68K_A_HL);
            break;

        case 0x2e: // ld l, imm8
            emit_move_w_an_dn(block, REG_68K_A_HL, REG_68K_D_SCRATCH_1);
            emit_move_b_dn(block, REG_68K_D_SCRATCH_1, gb_code[src_ptr++]);
            emit_movea_w_dn_an(block, REG_68K_D_SCRATCH_1, REG_68K_A_HL);
            break;

        case 0x3e: // ld a, imm8
            emit_moveq_dn(block, REG_68K_D_A, gb_code[src_ptr++]);
            break;

        case 0x10: // stop - end of execution
            emit_move_l_dn(block, REG_68K_D_NEXT_PC, 0xffffffff);
            emit_rts(block);
            done = 1;
            break;

        case 0x18: // jr disp8
            done = compile_jr(block, gb_code, &src_ptr, src_address);
            break;

        case 0x20: // jr nz, disp8
            compile_jr_cond(block, gb_code, &src_ptr, src_address, 7, 0);
            break;
        case 0x28: // jr z, disp8
            compile_jr_cond(block, gb_code, &src_ptr, src_address, 7, 1);
            break;
        case 0x30: // jr nc, disp8
            compile_jr_cond(block, gb_code, &src_ptr, src_address, 4, 0);
            break;
        case 0x38: // jr c, disp8
            compile_jr_cond(block, gb_code, &src_ptr, src_address, 4, 1);
            break;

        case 0x3d: // dec a
            emit_subq_b_dn(block, REG_68K_D_A, 1);
            compile_set_z_flag(block);
            emit_ori_b_dn(block, REG_68K_D_FLAGS, 0x40);  // D7 |= 0x40 (N flag)
            break;

        case 0xfe: // cp a, imm8
            emit_cmp_b_imm_dn(block, REG_68K_D_A, gb_code[src_ptr++]);
            compile_set_znc_flags(block);
            break;

        case 0xc3: // jp imm16
            {
                uint16_t target = gb_code[src_ptr] | (gb_code[src_ptr + 1] << 8);
                src_ptr += 2;
                emit_moveq_dn(block, REG_68K_D_NEXT_PC, 0);
                emit_move_w_dn(block, REG_68K_D_NEXT_PC, target);
                emit_rts(block);
                done = 1;
            }
            break;

        case 0xc9: // ret
            {
                // pop return address from stack (A1 = base + SP)
                // use byte operations to handle odd SP addresses
                emit_moveq_dn(block, REG_68K_D_NEXT_PC, 0);
                emit_move_b_disp_an_dn(block, 1, REG_68K_A_SP, REG_68K_D_NEXT_PC);  // D4 = [SP+1] (high byte)
                emit_rol_w_8(block, REG_68K_D_NEXT_PC);                  // shift to high position
                emit_move_b_ind_an_dn(block, REG_68K_A_SP, REG_68K_D_NEXT_PC);      // D4.b = [SP] (low byte)
                emit_addq_w_an(block, REG_68K_A_SP, 2);             // SP += 2
                emit_rts(block);
                done = 1;
            }
            break;

        case 0xcd: // call imm16
            {
                uint16_t target = gb_code[src_ptr] | (gb_code[src_ptr + 1] << 8);
                uint16_t ret_addr = src_address + src_ptr + 2;  // address after call
                src_ptr += 2;

                // push return address (A1 = base + SP)
                // use byte operations to handle odd SP addresses
                emit_moveq_dn(block, REG_68K_D_SCRATCH_1, 0);
                emit_move_w_dn(block, REG_68K_D_SCRATCH_1, ret_addr);
                emit_subq_w_an(block, REG_68K_A_SP, 2);             // SP -= 2
                emit_move_b_dn_ind_an(block, REG_68K_D_SCRATCH_1, REG_68K_A_SP);      // [SP] = low byte
                emit_rol_w_8(block, REG_68K_D_SCRATCH_1);                  // swap bytes
                emit_move_b_dn_disp_an(block, REG_68K_D_SCRATCH_1, 1, REG_68K_A_SP);  // [SP+1] = high byte

                // jump to target
                emit_moveq_dn(block, REG_68K_D_NEXT_PC, 0);
                emit_move_w_dn(block, REG_68K_D_NEXT_PC, target);
                emit_rts(block);
                done = 1;
            }
            break;

        default:
            // call interpreter as a fallback?
            fprintf(stderr, "compile_block: unknown opcode 0x%02x at 0x%04x\n",
                    op, src_address + src_ptr - 1);
            emit_move_l_dn(block, REG_68K_D_NEXT_PC, 0xffffffff);
            emit_rts(block);
            done = 1;
            break;
        }
    }

    block->end_address = src_address + src_ptr;
    return block;
}

void block_free(struct code_block *block)
{
    free(block);
}
