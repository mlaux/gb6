/* Game Boy emulator for 68k Macs
   Compiled with Symantec THINK C 5.0
   (c) 2013 Matt Laux
   
   z80.c - Game Boy CPU emulation */
   
#include <stdio.h>

#include "gb_types.h"
#include "mem_model.h"
#include "z80.h"

static u8 insn_cycles[] = { 0 };

static void set(z80_regs *regs, int flag) { regs->af.ind.f |= flag; }
static void clear(z80_regs *regs, int flag) { regs->af.ind.f &= ~flag; }

static void inc_with_carry(z80_regs *regs, u8 *reg)
{
	clear(regs, FLAG_SUBTRACT);
	if(*reg == 0xff || *reg == 0x0f)
		set(regs, FLAG_HALF_CARRY);
	(*reg)++;
	if(*reg == 0)
		set(regs, FLAG_ZERO);
}

static void dec_with_carry(z80_regs *regs, u8 *reg)
{
	set(regs, FLAG_SUBTRACT);
	if(*reg == 0x00 || *reg == 0x10)
		set(regs, FLAG_HALF_CARRY);
	(*reg)--;
	if(*reg == 0)
		set(regs, FLAG_ZERO);
}

static void rotate_left(z80_regs *regs, u8 *reg)
{
	// copy old leftmost bit to carry flag
	regs->af.ind.f = (*reg & 0x80) >> 3 | (regs->af.ind.f & ~FLAG_CARRY);
	// rotate
	*reg <<= 1;
	// restore leftmost (now rightmost) bit
	*reg |= (regs->af.ind.f & FLAG_CARRY) >> 4;
}

static void rotate_right(z80_regs *regs, u8 *reg)
{
	// copy old rightmost bit to carry flag
	regs->af.ind.f = (*reg & 0x01) << 4 | (regs->af.ind.f & ~FLAG_CARRY);
	// rotate
	*reg >>= 1;
	// restore rightmost bit to left
	*reg |= (regs->af.ind.f & FLAG_CARRY) << 3;
}

static void xor(z80_regs *regs, u8 value)
{
	regs->af.ind.a ^= value;
	if(regs->af.ind.a == 0)
		set(regs, FLAG_ZERO);
	clear(regs, FLAG_SUBTRACT);
	clear(regs, FLAG_HALF_CARRY);
	clear(regs, FLAG_CARRY);
}

z80_state *z80_create(void)
{
	z80_state *state;
	
	state = (z80_state *) NewPtr(sizeof(z80_state));
	
	state->regs = (z80_regs *) NewPtr(sizeof(z80_regs));
	state->ram = (u8 *) NewPtr(GB_RAM_SIZE);
	state->regs->pc = 0;
	
	return state;
}

void z80_destroy(z80_state *state)
{
	DisposPtr((char *) state->regs);
	DisposPtr((char *) state->ram);
	DisposPtr((char *) state);
}

void z80_dump_regs(z80_state *state)
{
	char line1[256], line2[256], line3[256], line4[256];
	sprintf(line1, " PC: %04x, Opcode: %02x", state->current_pc, state->current_op);
	sprintf(line2, " A: %02x, F: %02x, B: %02x, C: %02x", state->regs->af.ind.a,
			state->regs->af.ind.f, state->regs->bc.ind.b, state->regs->bc.ind.c);
	sprintf(line3, " D: %02x, E: %02x, H: %02x, L: %02x", state->regs->de.ind.d,
			state->regs->de.ind.e, state->regs->hl.ind.h, state->regs->hl.ind.l);
	sprintf(line4, " SP: %04x, PC: %04x", state->regs->sp, state->regs->pc);
	
	line1[0] = strlen(line1);
	line2[0] = strlen(line2);
	line3[0] = strlen(line3);
	line4[0] = strlen(line4);
	
	ParamText((void *) line1, (void *) line2, (void *) line3, (void *) line4);
	Alert(129, NULL);
}

#define load(dst, src) ((dst) = (src))

void z80_run(z80_state *state)
{
	z80_regs *regs = state->regs;
	u16 old;
	
	for(;;) {
		u8 op = mem_read(regs->pc);
		
		state->current_op = op;
		state->current_pc = regs->pc;
		
		z80_dump_regs(state);
		regs->pc++;
		
		switch(op) {
			// 8-bit immediate loads
			
			case 0x06: load(regs->bc.ind.b, mem_read(regs->pc)); regs->pc++; break;
			case 0x0e: load(regs->bc.ind.c, mem_read(regs->pc)); regs->pc++; break;
			case 0x16: load(regs->de.ind.d, mem_read(regs->pc)); regs->pc++; break;
			case 0x1e: load(regs->de.ind.e, mem_read(regs->pc)); regs->pc++; break;
			case 0x26: load(regs->hl.ind.h, mem_read(regs->pc)); regs->pc++; break;
			case 0x2e: load(regs->hl.ind.l, mem_read(regs->pc)); regs->pc++; break;
			case 0x36: mem_write(regs->hl.val, mem_read(regs->pc)); regs->pc++; break;
			case 0x3e: load(regs->af.ind.a, mem_read(regs->pc)); regs->pc++; break;
			
			// 8-bit register -> *register copies
			
			// src = A
			case 0x02: mem_write(regs->bc.val, regs->af.ind.a); break; // *BC = A
			case 0x12: mem_write(regs->de.val, regs->af.ind.a); break; // *DE = A
			case 0x22: mem_write(regs->hl.val, regs->af.ind.a); regs->hl.val++; break;
			case 0x32: mem_write(regs->hl.val, regs->af.ind.a); regs->hl.val--; break;
			
			// dest = A
			case 0x0a: load(regs->af.ind.a, mem_read(regs->bc.val)); break; // A = *BC
			case 0x1a: load(regs->af.ind.a, mem_read(regs->de.val)); break; // A = *DE
			case 0x2a: load(regs->af.ind.a, mem_read(regs->hl.val)); regs->hl.val++; break;
			case 0x3a: load(regs->af.ind.a, mem_read(regs->hl.val)); regs->hl.val--; break;
			
			// dest = *HL
			case 0x70: mem_write(regs->hl.val, regs->bc.ind.b); break;
			case 0x71: mem_write(regs->hl.val, regs->bc.ind.c); break;
			case 0x72: mem_write(regs->hl.val, regs->de.ind.d); break;
			case 0x73: mem_write(regs->hl.val, regs->de.ind.e); break;
			case 0x74: mem_write(regs->hl.val, regs->hl.ind.h); break;
			case 0x75: mem_write(regs->hl.val, regs->hl.ind.l); break;
			// 0x76 is HALT
			case 0x77: mem_write(regs->hl.val, regs->af.ind.a); break;
			
			// 8-bit register -> register copies
			
			// dest = A
			case 0x78: load(regs->af.ind.a, regs->bc.ind.b); break;
			case 0x79: load(regs->af.ind.a, regs->bc.ind.c); break;
			case 0x7a: load(regs->af.ind.a, regs->de.ind.d); break;
			case 0x7b: load(regs->af.ind.a, regs->de.ind.e); break;
			case 0x7c: load(regs->af.ind.a, regs->hl.ind.h); break;
			case 0x7d: load(regs->af.ind.a, regs->hl.ind.l); break;
			case 0x7e: load(regs->af.ind.a, mem_read(regs->hl.val)); break;
			case 0x7f: break; // copy A to A
			
			// dest = B
			case 0x40: break; // copy B to B
			case 0x41: load(regs->bc.ind.b, regs->bc.ind.c); break;
			case 0x42: load(regs->bc.ind.b, regs->de.ind.d); break;
			case 0x43: load(regs->bc.ind.b, regs->de.ind.e); break;
			case 0x44: load(regs->bc.ind.b, regs->hl.ind.h); break;
			case 0x45: load(regs->bc.ind.b, regs->hl.ind.l); break;
			case 0x46: load(regs->bc.ind.b, mem_read(regs->hl.val)); break;
			case 0x47: load(regs->bc.ind.b, regs->af.ind.a); break;
			
			// dest = C
			case 0x48: load(regs->bc.ind.c, regs->bc.ind.b); break;
			case 0x49: break; // copy C to C
			case 0x4a: load(regs->bc.ind.c, regs->de.ind.d); break;
			case 0x4b: load(regs->bc.ind.c, regs->de.ind.e); break;
			case 0x4c: load(regs->bc.ind.c, regs->hl.ind.h); break;
			case 0x4d: load(regs->bc.ind.c, regs->hl.ind.l); break;
			case 0x4e: load(regs->bc.ind.c, mem_read(regs->hl.val)); break;
			case 0x4f: load(regs->bc.ind.c, regs->af.ind.a); break;
			
			// dest = D
			case 0x50: load(regs->de.ind.d, regs->bc.ind.b); break;
			case 0x51: load(regs->de.ind.d, regs->bc.ind.c); break;
			case 0x52: break; // copy D to D
			case 0x53: load(regs->de.ind.d, regs->de.ind.e); break;
			case 0x54: load(regs->de.ind.d, regs->hl.ind.h); break;
			case 0x55: load(regs->de.ind.d, regs->hl.ind.l); break;
			case 0x56: load(regs->de.ind.d, mem_read(regs->hl.val)); break;
			case 0x57: load(regs->de.ind.d, regs->af.ind.a); break;
			
			// dest = E
			case 0x58: load(regs->de.ind.e, regs->bc.ind.b); break;
			case 0x59: load(regs->de.ind.e, regs->bc.ind.c); break;
			case 0x5a: load(regs->de.ind.e, regs->de.ind.d); break;
			case 0x5b: break; // copy E to E
			case 0x5c: load(regs->de.ind.e, regs->hl.ind.h); break;
			case 0x5d: load(regs->de.ind.e, regs->hl.ind.l); break;
			case 0x5e: load(regs->de.ind.e, mem_read(regs->hl.val)); break;
			case 0x5f: load(regs->de.ind.e, regs->af.ind.a); break;
			
			// dest = H
			case 0x60: load(regs->hl.ind.h, regs->bc.ind.b); break;
			case 0x61: load(regs->hl.ind.h, regs->bc.ind.c); break;
			case 0x62: load(regs->hl.ind.h, regs->de.ind.d); break;
			case 0x63: load(regs->hl.ind.h, regs->de.ind.e); break;
			case 0x64: break; // copy H to H
			case 0x65: load(regs->hl.ind.h, regs->hl.ind.l); break;
			case 0x66: load(regs->hl.ind.h, mem_read(regs->hl.val)); break;
			case 0x67: load(regs->hl.ind.h, regs->af.ind.a); break;
			
			// dest = L
			case 0x68: load(regs->hl.ind.l, regs->bc.ind.b); break;
			case 0x69: load(regs->hl.ind.l, regs->bc.ind.c); break;
			case 0x6a: load(regs->hl.ind.l, regs->de.ind.d); break;
			case 0x6b: load(regs->hl.ind.l, regs->de.ind.e); break;
			case 0x6c: load(regs->hl.ind.l, regs->hl.ind.h); break;
			case 0x6d: break; // copy L to L
			case 0x6e: load(regs->hl.ind.l, mem_read(regs->hl.val)); break;
			case 0x6f: load(regs->hl.ind.l, regs->af.ind.a); break;
			
			case 0x00: break; // NOP
			case 0x01: // LD BC, 0xNNNN
				regs->bc.val = mem_read_word(regs->pc);
				regs->pc += 2;
				break;
			case 0x03: // INC BC
				regs->bc.val++;
				break;
			case 0x04: // INC B
				inc_with_carry(regs, &regs->bc.ind.b);
				break;
			case 0x05: // DEC B
				dec_with_carry(regs, &regs->bc.ind.b);
				break;
			case 0x07: // RLCA
				rotate_left(regs, &regs->af.ind.a);
				break;
			case 0x08: // LD (0xNNNN), SP
				mem_write_word(regs->pc, regs->sp);
				regs->pc += 2;
				break;
			case 0x09: // ADD HL, BC
				clear(regs, FLAG_SUBTRACT);
				old = regs->hl.val;
				regs->hl.val += regs->bc.val;
				if(regs->hl.val < old) // overflow occured
					set(regs, FLAG_CARRY);
				if(regs->hl.val >= 0x1000) // half carry on high byte
					set(regs, FLAG_HALF_CARRY);
				break;
			case 0x0b: // DEC BC
				regs->bc.val--;
				break;
			case 0x0c: // INC C
				inc_with_carry(regs, &regs->bc.ind.c);
				break;
			case 0x0d: // DEC C
				dec_with_carry(regs, &regs->bc.ind.c);
				break;
			case 0x0f: // RRCA
				rotate_right(regs, &regs->af.ind.a);
				break;
			case 0x10: // STOP
				// 2 bytes long for some reason
				regs->pc++;
				return;
				break;
			case 0x11: // LD DE, 0xNNNN
				regs->de.val = mem_read_word(regs->pc);
				regs->pc += 2;
				break;
				
			case 0xc3:
				regs->pc = mem_read_word(regs->pc);
				break;
				
			// XOR
			case 0xaf: xor(regs, regs->af.ind.a); break;
			case 0xa8: xor(regs, regs->bc.ind.b); break;
			case 0xa9: xor(regs, regs->bc.ind.c); break;
			case 0xaa: xor(regs, regs->de.ind.d); break;
			case 0xab: xor(regs, regs->de.ind.e); break;
			case 0xac: xor(regs, regs->hl.ind.h); break;
			case 0xad: xor(regs, regs->hl.ind.l); break;
			case 0xae: xor(regs, mem_read(regs->hl.val)); break;
			case 0xee: xor(regs, mem_read(regs->pc)); regs->pc++; break;
			
			default:
				break;
			
		}
	}
}