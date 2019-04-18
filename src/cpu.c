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
    printf("sp=%04x pc=%04x\n", cpu->sp, cpu->pc);
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

static void xor(struct cpu *regs, u8 value)
{
	regs->a ^= value;
	if(regs->a == 0)
		set_flag(regs, FLAG_ZERO);
	clear_flag(regs, FLAG_SIGN);
	clear_flag(regs, FLAG_HALF_CARRY);
	clear_flag(regs, FLAG_CARRY);
}

void cpu_step(struct cpu *cpu)
{
    u8 opc = cpu->mem_read(cpu->mem_model, cpu->pc);
    cpu->pc++;
    switch (opc) {
        case 0: // NOP
            break;
        case 0x01: // LD BC, 0xNNNN
            write_bc(cpu, read16(cpu, cpu->pc));
            cpu->pc += 2;
            break;
        case 0x03: // INC BC
            write_bc(cpu, read_bc(cpu) + 1);
            break;
        case 0x04: // INC B
            inc_with_carry(cpu, &cpu->b);
            break;
        case 0x05: // DEC B
            dec_with_carry(cpu, &cpu->b);
            break;
        case 0x06: // LD B, d8
            cpu->b = read8(cpu, cpu->pc);
            cpu->pc += 1;
            break;
        case 0x0e: // LD C, d8
            cpu->c = read8(cpu, cpu->pc);
            cpu->pc += 1;
            break;
        case 0x21: // LD HL, d16
            write_hl(cpu, read16(cpu, cpu->pc));
            cpu->pc += 2;
            break;
        case 0x32: // LD (HL-), A
            write8(cpu, read_hl(cpu), cpu->a);
            write_hl(cpu, read_hl(cpu) - 1);
            break;
        case 0xc3: // JP a16
            cpu->pc = read16(cpu, cpu->pc);
            break;
        case 0xaf: // XOR A
            cpu->a = 0;
            break;
        default:
            printf("unknown opcode 0x%02x %s\n", opc, instructions[opc].format);
            cpu_panic(cpu);
    }
}
