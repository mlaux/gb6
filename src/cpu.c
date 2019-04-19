#include <stdio.h>
#include <stdlib.h>

#include "cpu.h"
#include "types.h"
#include "instructions.h"

void cpu_bind_mem_model(
    struct cpu *cpu,
    void *mem_model,
    u8 (*mem_read)(void *, u16),
    void (*mem_write)(void *, u16, u8)
) {
    cpu->mem_model = mem_model;
    cpu->mem_read = mem_read;
    cpu->mem_write = mem_write;
}

static inline int flag_isset(struct cpu *cpu, int flag)
{
    return (cpu->f & flag) != 0;
}

static inline void set_flag(struct cpu *cpu, int flag)
{
    cpu->f |= flag;
}

static inline void clear_flag(struct cpu *cpu, int flag)
{
    cpu->f &= ~flag;
}

static inline u16 read_af(struct cpu *cpu)
{
    return cpu->a << 8 | cpu->f;
}

static inline u16 read_bc(struct cpu *cpu)
{
    return cpu->b << 8 | cpu->c;
}

static inline u16 read_de(struct cpu *cpu)
{
    return cpu->d << 8 | cpu->e;
}

static inline u16 read_hl(struct cpu *cpu)
{
    return cpu->h << 8 | cpu->l;
}

static inline u16 read_double_reg(struct cpu *cpu, u8 *rh, u8 *rl)
{
    return *rh << 8 | *rl;
}

static inline void write_af(struct cpu *cpu, int value)
{
    cpu->a = value >> 8;
    cpu->f = value & 0xff;
}

static inline void write_bc(struct cpu *cpu, int value)
{
    cpu->b = value >> 8;
    cpu->c = value & 0xff;
}

static inline void write_de(struct cpu *cpu, int value)
{
    cpu->d = value >> 8;
    cpu->e = value & 0xff;
}

// TODO figure out if I like this style better and convert write_af, etc to this
static inline void write_double_reg(struct cpu *cpu, u8 *rh, u8 *rl, int value)
{
    *rh = value >> 8;
    *rl = value & 0xff;
}

#define write_hl(cpu, value) write_double_reg((cpu), &cpu->h, &cpu->l, value)

void cpu_panic(struct cpu *cpu)
{
    printf("a=%02x f=%02x b=%02x c=%02x\n", cpu->a, cpu->f, cpu->b, cpu->c);
    printf("d=%02x e=%02x h=%02x l=%02x\n", cpu->d, cpu->e, cpu->h, cpu->l);
    printf("sp=%04x pc=%04x flags=%s%s%s%s\n", cpu->sp, cpu->pc,
            flag_isset(cpu, FLAG_ZERO) ? "Z" : "-",
            flag_isset(cpu, FLAG_SIGN) ? "S" : "-",
            flag_isset(cpu, FLAG_HALF_CARRY) ? "H" : "-",
            flag_isset(cpu, FLAG_CARRY) ? "C" : "-");
    exit(0);
}

static inline u8 read8(struct cpu *cpu, u16 address)
{
    return cpu->mem_read(cpu->mem_model, address);
}

static inline u16 read16(struct cpu *cpu, u16 address)
{
    u8 low = read8(cpu, address);
    u8 high = read8(cpu, address + 1);
    return high << 8 | low;
}

static inline void write8(struct cpu *cpu, u16 address, u8 data)
{
    cpu->mem_write(cpu->mem_model, address, data);
}

static inline void write16(struct cpu *cpu, u16 address, u16 data)
{
    cpu->mem_write(cpu->mem_model, address, data);
}

static void inc_with_carry(struct cpu *regs, u8 *reg)
{
	clear_flag(regs, FLAG_SIGN);
	if(*reg == 0xff || *reg == 0x0f)
		set_flag(regs, FLAG_HALF_CARRY);
	(*reg)++;
	if(*reg == 0)
		set_flag(regs, FLAG_ZERO);
}

static void dec_with_carry(struct cpu *regs, u8 *reg)
{
	set_flag(regs, FLAG_SIGN);
	if(*reg == 0x00 || *reg == 0x10)
		set_flag(regs, FLAG_HALF_CARRY);
	(*reg)--;
	if(*reg == 0)
		set_flag(regs, FLAG_ZERO);
}

static void rotate_left(struct cpu *regs, u8 *reg)
{
	// copy old leftmost bit to carry flag
	regs->f = (*reg & 0x80) >> 3 | (regs->f & ~FLAG_CARRY);
	// rotate
	*reg <<= 1;
	// restore leftmost (now rightmost) bit
	*reg |= (regs->f & FLAG_CARRY) >> 4;
}

static void rotate_right(struct cpu *regs, u8 *reg)
{
	// copy old rightmost bit to carry flag
	regs->f = (*reg & 0x01) << 4 | (regs->f & ~FLAG_CARRY);
	// rotate
	*reg >>= 1;
	// restore rightmost bit to left
	*reg |= (regs->f & FLAG_CARRY) << 3;
}

static void subtract(struct cpu *cpu, u8 value)
{
    cpu->a -= value;
    if (cpu->a > 0x80) {
        set_flag(cpu, FLAG_SIGN);
    }
}

static void xor(struct cpu *regs, u8 value)
{
	regs->a ^= value;
	if(regs->a == 0)
		set_flag(regs, FLAG_ZERO);
	clear_flag(regs, FLAG_SIGN);
	clear_flag(regs, FLAG_HALF_CARRY);
	clear_flag(regs, FLAG_CARRY);
}

static void push(struct cpu *cpu, u16 value)
{
    write16(cpu, cpu->sp - 2, value & 0xff);
    write16(cpu, cpu->sp - 1, value >> 8);
    cpu->sp -= 2;
}

static u16 pop(struct cpu *cpu)
{
    cpu->sp += 2;
    return read16(cpu, cpu->sp - 1) << 8 | read16(cpu, cpu->sp - 2);
}

void cpu_step(struct cpu *cpu)
{
    u8 temp;
    u8 opc = cpu->mem_read(cpu->mem_model, cpu->pc);

    cpu->pc++;
    switch (opc) {
        case 0: // NOP
            break;
        case 0x01: // LD BC, 0xNNNN
            write_bc(cpu, read16(cpu, cpu->pc));
            cpu->pc += 2;
            break;

        // incs and decs
        case 0x03: write_bc(cpu, read_bc(cpu) + 1); break;
        case 0x04: inc_with_carry(cpu, &cpu->b); break;
        case 0x05: dec_with_carry(cpu, &cpu->b); break;

        case 0x0b: write_bc(cpu, read_bc(cpu) - 1); break;
        case 0x0c: inc_with_carry(cpu, &cpu->c); break;
        case 0x0d: dec_with_carry(cpu, &cpu->c); break;

        case 0x13: write_de(cpu, read_de(cpu) + 1); break;
        case 0x14: inc_with_carry(cpu, &cpu->d); break;
        case 0x15: dec_with_carry(cpu, &cpu->d); break;

        case 0x1b: write_de(cpu, read_de(cpu) - 1); break;
        case 0x1c: inc_with_carry(cpu, &cpu->e); break;
        case 0x1d: dec_with_carry(cpu, &cpu->e); break;

        case 0x23: write_hl(cpu, read_hl(cpu) + 1); break;
        case 0x24: inc_with_carry(cpu, &cpu->h); break;
        case 0x25: dec_with_carry(cpu, &cpu->h); break;

        case 0x2b: write_hl(cpu, read_hl(cpu) - 1); break;
        case 0x2c: inc_with_carry(cpu, &cpu->l); break;
        case 0x2d: dec_with_carry(cpu, &cpu->l); break;

        case 0x33: cpu->sp++; break;
        case 0x34: temp = read8(cpu, read_hl(cpu)); inc_with_carry(cpu, &temp); break;
        case 0x35: temp = read8(cpu, read_hl(cpu)); dec_with_carry(cpu, &temp); break;

        case 0x3b: cpu->sp--; break;
        case 0x3c: inc_with_carry(cpu, &cpu->a); break;
        case 0x3d: dec_with_carry(cpu, &cpu->a); break;

        // 8-bit immediate loads
        case 0x06: cpu->b = read8(cpu, cpu->pc); cpu->pc++; break;
        case 0x0e: cpu->c = read8(cpu, cpu->pc); cpu->pc++; break;
        case 0x16: cpu->d = read8(cpu, cpu->pc); cpu->pc++; break;
        case 0x1e: cpu->e = read8(cpu, cpu->pc); cpu->pc++; break;
        case 0x26: cpu->h = read8(cpu, cpu->pc); cpu->pc++; break;
        case 0x2e: cpu->l = read8(cpu, cpu->pc); cpu->pc++; break;
        case 0x36: write8(cpu, read_hl(cpu), read8(cpu, cpu->pc)); cpu->pc++; break;
        case 0x3e: cpu->a = read8(cpu, cpu->pc); cpu->pc++; break;

        // 8-bit register -> register copies
        // dest = B
        case 0x40: break; // copy B to B
        case 0x41: cpu->b = cpu->c; break;
        case 0x42: cpu->b = cpu->d; break;
        case 0x43: cpu->b = cpu->e; break;
        case 0x44: cpu->b = cpu->h; break;
        case 0x45: cpu->b = cpu->l; break;
        case 0x46: cpu->b = read8(cpu, read_hl(cpu)); break;
        case 0x47: cpu->b = cpu->a; break;
        
        // dest = C
        case 0x48: cpu->c = cpu->b; break;
        case 0x49: break; // copy C to C
        case 0x4a: cpu->c = cpu->d; break;
        case 0x4b: cpu->c = cpu->e; break;
        case 0x4c: cpu->c = cpu->h; break;
        case 0x4d: cpu->c = cpu->l; break;
        case 0x4e: cpu->c = read8(cpu, read_hl(cpu)); break;
        case 0x4f: cpu->c = cpu->a; break;
        
        // dest = D
        case 0x50: cpu->d = cpu->b; break;
        case 0x51: cpu->d = cpu->c; break;
        case 0x52: break; // copy D to D
        case 0x53: cpu->d = cpu->e; break;
        case 0x54: cpu->d = cpu->h; break;
        case 0x55: cpu->d = cpu->l; break;
        case 0x56: cpu->d = read8(cpu, read_hl(cpu)); break;
        case 0x57: cpu->d = cpu->a; break;
        
        // dest = E
        case 0x58: cpu->e = cpu->b; break;
        case 0x59: cpu->e = cpu->c; break;
        case 0x5a: cpu->e = cpu->d; break;
        case 0x5b: break; // copy E to E
        case 0x5c: cpu->e = cpu->h; break;
        case 0x5d: cpu->e = cpu->l; break;
        case 0x5e: cpu->e = read8(cpu, read_hl(cpu)); break;
        case 0x5f: cpu->e = cpu->a; break;
        
        // dest = H
        case 0x60: cpu->h = cpu->b; break;
        case 0x61: cpu->h = cpu->c; break;
        case 0x62: cpu->h = cpu->d; break;
        case 0x63: cpu->h = cpu->e; break;
        case 0x64: break; // copy H to H
        case 0x65: cpu->h = cpu->l; break;
        case 0x66: cpu->h = read8(cpu, read_hl(cpu)); break;
        case 0x67: cpu->h = cpu->a; break;
        
        // dest = L
        case 0x68: cpu->l = cpu->b; break;
        case 0x69: cpu->l = cpu->c; break;
        case 0x6a: cpu->l = cpu->d; break;
        case 0x6b: cpu->l = cpu->e; break;
        case 0x6c: cpu->l = cpu->h; break;
        case 0x6d: break; // copy L to L
        case 0x6e: cpu->l = read8(cpu, read_hl(cpu)); break;
        case 0x6f: cpu->l = cpu->a; break;
                
        // dest = *HL
        case 0x70: write8(cpu, read_hl(cpu), cpu->b); break;
        case 0x71: write8(cpu, read_hl(cpu), cpu->c); break;
        case 0x72: write8(cpu, read_hl(cpu), cpu->d); break;
        case 0x73: write8(cpu, read_hl(cpu), cpu->e); break;
        case 0x74: write8(cpu, read_hl(cpu), cpu->h); break;
        case 0x75: write8(cpu, read_hl(cpu), cpu->l); break;
        // 0x76 is HALT
        case 0x77: write8(cpu, read_hl(cpu), cpu->a); break;
        
        // dest = A
        case 0x78: cpu->a = cpu->b; break;
        case 0x79: cpu->a = cpu->c; break;
        case 0x7a: cpu->a = cpu->d; break;
        case 0x7b: cpu->a = cpu->e; break;
        case 0x7c: cpu->a = cpu->h; break;
        case 0x7d: cpu->a = cpu->l; break;
        case 0x7e: cpu->a = read8(cpu, read_hl(cpu)); break;
        case 0x7f: break; // copy A to A

        // A = r16
        case 0x0a: cpu->a = read8(cpu, read_bc(cpu)); break; // A = *BC
        case 0x1a: cpu->a = read8(cpu, read_de(cpu)); break; // A = *DE
        case 0x2a: cpu->a = read8(cpu, read_hl(cpu)); write_hl(cpu, read_hl(cpu) + 1); break;
        case 0x3a: cpu->a = read8(cpu, read_hl(cpu)); write_hl(cpu, read_hl(cpu) - 1); break;

        case 0x20: // JR NZ,r8
            temp = read8(cpu, cpu->pc);
            if (!flag_isset(cpu, FLAG_ZERO)) {
                cpu->pc += *((signed char *) &temp);
            } else {
                cpu->pc++;
            }
            break;
        case 0x21: // LD HL, d16
            write_hl(cpu, read16(cpu, cpu->pc));
            cpu->pc += 2;
            break;
        case 0x31: // LD SP,d16
            cpu->sp = read16(cpu, cpu->pc);
            cpu->pc += 2;
            break;
        case 0x32: // LD (HL-), A
            write8(cpu, read_hl(cpu), cpu->a);
            write_hl(cpu, read_hl(cpu) - 1);
            break;
        case 0x94:
            subtract(cpu, cpu->h);
            break;
        case 0xcd: // CALL a16
            temp = read16(cpu, cpu->pc);
            cpu->pc += 2;
            push(cpu, cpu->pc);
            cpu->pc = temp;
            break;
        case 0xc3: // JP a16
            cpu->pc = read16(cpu, cpu->pc);
            break;

        // XOR
        case 0xa8: xor(cpu, cpu->b); break;
        case 0xa9: xor(cpu, cpu->c); break;
        case 0xaa: xor(cpu, cpu->d); break;
        case 0xab: xor(cpu, cpu->e); break;
        case 0xac: xor(cpu, cpu->h); break;
        case 0xad: xor(cpu, cpu->l); break;
        case 0xae: xor(cpu, read8(cpu, read_hl(cpu))); break;
        case 0xaf: xor(cpu, cpu->a); break;
        case 0xee: xor(cpu, read8(cpu, cpu->pc)); cpu->pc++; break;
        
        case 0xe0: // LDH (a8),A
            write16(cpu, 0xff00 + read8(cpu, cpu->pc), cpu->a);
            cpu->pc++;
            break;
        case 0xe2: // LD (C),A
            write8(cpu, cpu->c, cpu->a);
            break;
        case 0xea: // LD (a16),A
            write16(cpu, read16(cpu, cpu->pc), cpu->a);
            cpu->pc += 2;
            break;
        case 0xf0: // LDH A,(a8)
            cpu->a = read16(cpu, 0xff00 + read8(cpu, cpu->pc));
            cpu->pc++;
        case 0xfe: // CP d8
            if (cpu->a - read8(cpu, cpu->pc) > 0x80) {
                set_flag(cpu, FLAG_SIGN);
            }
            cpu->pc++;
            break;
        case 0xf3: // DI
            printf("disable interrupts\n");
            break;
        default:
            printf("unknown opcode 0x%02x %s\n", opc, instructions[opc].format);
            cpu_panic(cpu);
    }
}
