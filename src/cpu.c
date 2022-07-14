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

int flag_isset(struct cpu *cpu, int flag)
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

static inline u16 read_double_reg(struct cpu *cpu, u8 *rh, u8 *rl)
{
    return *rh << 8 | *rl;
}

#define read_af(cpu) read_double_reg((cpu), &(cpu)->a, &(cpu)->f)
#define read_bc(cpu) read_double_reg((cpu), &(cpu)->b, &(cpu)->c)
#define read_de(cpu) read_double_reg((cpu), &(cpu)->d, &(cpu)->e)
#define read_hl(cpu) read_double_reg((cpu), &(cpu)->h, &(cpu)->l)

static inline void write_double_reg(struct cpu *cpu, u8 *rh, u8 *rl, int value)
{
    *rh = value >> 8;
    *rl = value & 0xff;
}

#define write_af(cpu, value) write_double_reg((cpu), &(cpu)->a, &(cpu)->f, value)
#define write_bc(cpu, value) write_double_reg((cpu), &(cpu)->b, &(cpu)->c, value)
#define write_de(cpu, value) write_double_reg((cpu), &(cpu)->d, &(cpu)->e, value)
#define write_hl(cpu, value) write_double_reg((cpu), &(cpu)->h, &(cpu)->l, value)

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
    else clear_flag(regs, FLAG_ZERO);
}

static void dec_with_carry(struct cpu *regs, u8 *reg)
{
	set_flag(regs, FLAG_SIGN);
	if(*reg == 0x00 || *reg == 0x10)
		set_flag(regs, FLAG_HALF_CARRY);
	(*reg)--;
	if(*reg == 0)
		set_flag(regs, FLAG_ZERO);
    else clear_flag(regs, FLAG_ZERO);
}

static u8 rotate_left(struct cpu *regs, u8 reg)
{
    int old_carry = flag_isset(regs, FLAG_CARRY);
	// copy old leftmost bit to carry flag, clear Z, N, H
	regs->f = (reg & 0x80) >> 3;
	// rotate
	int result = reg << 1 | old_carry;
    if (!result) set_flag(regs, FLAG_ZERO);
    return result;
}

static u8 rlc(struct cpu *cpu, u8 val)
{
    int old_msb = (val & 0x80) >> 7;
    int result = (val & 0x7f) << 1 | old_msb;
    if (!result)
        set_flag(cpu, FLAG_ZERO);
    else clear_flag(cpu, FLAG_ZERO);
    clear_flag(cpu, FLAG_SIGN);
    clear_flag(cpu, FLAG_HALF_CARRY);
    if (old_msb) 
        set_flag(cpu, FLAG_CARRY);
    else clear_flag(cpu, FLAG_CARRY);
    return result;
}

static u8 rotate_right(struct cpu *regs, u8 reg)
{
	// copy old rightmost bit to carry flag
	regs->f = (reg & 0x01) << 4;
	// rotate
	int result = reg >> 1;
	// restore rightmost bit to left
	result |= (regs->f & FLAG_CARRY) << 3;
    return result;
}

static u8 rrc(struct cpu *cpu, u8 val)
{
    int old_lsb = (val & 1) << 7;
    int result = old_lsb | (val & 0xfe) >> 1;
    if (!result)
        set_flag(cpu, FLAG_ZERO);
    else clear_flag(cpu, FLAG_ZERO);
    clear_flag(cpu, FLAG_SIGN);
    clear_flag(cpu, FLAG_HALF_CARRY);
    if (old_lsb) 
        set_flag(cpu, FLAG_CARRY);
    else clear_flag(cpu, FLAG_CARRY);
    return result;
}

static u8 shift_left(struct cpu *cpu, u8 value)
{
    return 0;
}

static u8 shift_right(struct cpu *cpu, u8 value)
{
    return 0;
}

static u8 swap(struct cpu *cpu, u8 value)
{
    return ((value & 0xf0) >> 4) | ((value & 0x0f) << 4);
}

static void xor(struct cpu *regs, u8 value)
{
	regs->a ^= value;
	if(regs->a == 0)
		set_flag(regs, FLAG_ZERO);
    else clear_flag(regs, FLAG_ZERO);
	clear_flag(regs, FLAG_SIGN);
	clear_flag(regs, FLAG_HALF_CARRY);
	clear_flag(regs, FLAG_CARRY);
}

static void or(struct cpu *regs, u8 value)
{
	regs->a |= value;
	if(regs->a == 0)
		set_flag(regs, FLAG_ZERO);
    else clear_flag(regs, FLAG_ZERO);
	clear_flag(regs, FLAG_SIGN);
	clear_flag(regs, FLAG_HALF_CARRY);
	clear_flag(regs, FLAG_CARRY);
}

static void and(struct cpu *cpu, u8 value)
{
    cpu->a &= value;
	if(cpu->a == 0) {
		set_flag(cpu, FLAG_ZERO);
    } else {
        clear_flag(cpu, FLAG_ZERO);
    }
    clear_flag(cpu, FLAG_SIGN);
    set_flag(cpu, FLAG_HALF_CARRY);
    clear_flag(cpu, FLAG_CARRY);
}

static void add(struct cpu *cpu, u8 value, int with_carry)
{
    u8 sum_trunc;
    int sum_full = cpu->a + value;
    if (with_carry && flag_isset(cpu, FLAG_CARRY)) {
        sum_full++;
    }
    sum_trunc = (u8) sum_full;
    set_flag(cpu, sum_trunc == 0 ? FLAG_ZERO : 0);
    clear_flag(cpu, FLAG_SIGN);
    set_flag(cpu, sum_full > sum_trunc ? FLAG_CARRY : 0);
    // TODO H
    cpu->a = sum_trunc;
}

static void subtract(struct cpu *cpu, u8 value, int with_carry, int just_compare)
{
    u8 sum_trunc;
    int sum_full = cpu->a - value;
    if (with_carry && flag_isset(cpu, FLAG_CARRY)) {
        sum_full--;
    }
    sum_trunc = (u8) (sum_full & 0xff);
    if (!sum_trunc) {
        set_flag(cpu, FLAG_ZERO);
    } else {
        clear_flag(cpu, FLAG_ZERO);
    }
    set_flag(cpu, FLAG_SIGN);
    if (sum_full < sum_trunc) {
        set_flag(cpu, FLAG_CARRY);
    } else {
        clear_flag(cpu, FLAG_CARRY);
    }
    // TODO H

    if (!just_compare) {
        cpu->a = sum_trunc;
    }
}

static void push(struct cpu *cpu, u16 value)
{
    // todo #if DEBUG or something
    //printf("sp=%04x\n", cpu->sp);
    //printf("memory[sp-2] = %02x\n", value & 0xff);
    //printf("memory[sp-1] = %02x\n", value >> 8);
    write8(cpu, cpu->sp - 2, value & 0xff);
    write8(cpu, cpu->sp - 1, value >> 8);
    cpu->sp -= 2;
}

static u16 pop(struct cpu *cpu)
{
    cpu->sp += 2;
    //printf("sp=%04x\n", cpu->sp);
    //printf("read memory[sp-2] = %02x\n", read8(cpu, cpu->sp - 2));
    //printf("read memory[sp-1] = %02x\n", read8(cpu, cpu->sp - 1));
    return read8(cpu, cpu->sp - 1) << 8 | read8(cpu, cpu->sp - 2);
}

static void add16(struct cpu *cpu, u16 src)
{
    clear_flag(cpu, FLAG_SIGN);
    int total = read_hl(cpu) + src; // promoted to int
    if (total > 0xffff) {
        set_flag(cpu, FLAG_CARRY);
    } else {
        clear_flag(cpu, FLAG_CARRY);
    }
    if (((cpu->h & 0xf) + ((src >> 8) & 0xf)) & 0x10) {
        // true if carry from bit 11 to bit 12
        set_flag(cpu, FLAG_HALF_CARRY);
    } else {
        clear_flag(cpu, FLAG_HALF_CARRY);
    }

    write_hl(cpu, total & 0xffff);
}

static u8 read_reg(struct cpu *cpu, int index)
{
    switch (index) {
        case 0: return cpu->b;
        case 1: return cpu->c;
        case 2: return cpu->d;
        case 3: return cpu->e;
        case 4: return cpu->h;
        case 5: return cpu->l;
        case 6: return read8(cpu, read_hl(cpu));
        case 7: return cpu->a;
        default: cpu_panic(cpu);
    }

    // unreachable
    return 0;
}

static u8 write_reg(struct cpu *cpu, int index, u8 val)
{
    switch (index) {
        case 0: cpu->b = val; break;
        case 1: cpu->c = val; break;
        case 2: cpu->d = val; break;
        case 3: cpu->e = val; break;
        case 4: cpu->h = val; break;
        case 5: cpu->l = val; break;
        case 6: write8(cpu, read_hl(cpu), val); break;
        case 7: cpu->a = val; break;
        default: cpu_panic(cpu);
    }

    // unreachable
    return 0;
}

static void extended_insn(struct cpu *cpu, u8 insn)
{
    u8 temp;
    int op = insn >> 6;
    int bit = (insn >> 3) & 0x7;
    int reg = insn & 0x7;

    u8 (*funcs[8])(struct cpu *, u8) = {
        rlc,
        rrc,
        rotate_left,
        rotate_right,
        shift_left,
        shift_right,
        swap,
        shift_right // TODO SRL
    };

#ifdef GB6_DEBUG
    printf("       %s\n", instructions[insn + 0x100].format);
#endif
    cpu->cycle_count += instructions[insn + 0x100].cycles;

    switch (op) {
        case 0:
            write_reg(cpu, reg, funcs[bit](cpu, read_reg(cpu, reg)));
            break;
        case 1: // BIT
            temp = read_reg(cpu, reg);
            if ((temp & (1 << bit)) == 0) {
                set_flag(cpu, FLAG_ZERO);
            } else {
                clear_flag(cpu, FLAG_ZERO);
            }
            clear_flag(cpu, FLAG_SIGN);
            set_flag(cpu, FLAG_HALF_CARRY);
            break;
        case 2: // RES
            write_reg(cpu, reg, read_reg(cpu, reg) & ~(1 << bit));
            break;
        case 3: // SET
            write_reg(cpu, reg, read_reg(cpu, reg) | (1 << bit));
            break;
    }
}

void cpu_step(struct cpu *cpu)
{
    u8 temp;
    u16 temp16;
    u8 opc = cpu->mem_read(cpu->mem_model, cpu->pc);
#ifdef GB6_DEBUG
    printf("0x%04x %s\n", cpu->pc, instructions[opc].format);
#endif
    cpu->pc++;
    cpu->cycle_count += instructions[opc].cycles;
    switch (opc) {
        case 0: // NOP
            break;
        case 0x01: // LD BC, 0xNNNN
            write_bc(cpu, read16(cpu, cpu->pc));
            cpu->pc += 2;
            break;
        case 0x11: // LD DE,d16
            write_de(cpu, read16(cpu, cpu->pc));
            cpu->pc += 2;
            break;
        case 0x07: // RLCA
            cpu->a = rlc(cpu, cpu->a);
            break;
        case 0x08: // LD (a16),SP
            write16(cpu, read16(cpu, cpu->pc), cpu->sp);
            cpu->pc += 2;
            break;
        case 0x19: // ADD HL,DE
            add16(cpu, read_de(cpu));
            break;
        case 0x17: // RLA
            cpu->a = rotate_left(cpu, cpu->a);
            break;
        case 0x1f: // RRA
            cpu->a = rotate_right(cpu, cpu->a);
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

        case 0x12: // LD (DE),A
            write8(cpu, read_de(cpu), cpu->a);
            break;
        case 0x18: // JR r8
            temp = read8(cpu, cpu->pc);
            cpu->pc += *((signed char *) &temp) + 1;
            break;
        case 0x20: // JR NZ,r8
        case 0x28: // JR Z,r8
            temp = read8(cpu, cpu->pc);
            if ((opc == 0x20) ^ flag_isset(cpu, FLAG_ZERO)) {
                cpu->pc += *((signed char *) &temp) + 1;
                cpu->cycle_count += instructions[opc].cycles_branch - instructions[opc].cycles;
            } else {
                cpu->pc++;
            }
            break;
        case 0x21: // LD HL, d16
            write_hl(cpu, read16(cpu, cpu->pc));
            cpu->pc += 2;
            break;
        case 0x29: // ADD HL,HL
            add16(cpu, read_hl(cpu));
            break;
        case 0x2f: // CPL
            cpu->a = ~cpu->a;
            break;
        case 0x31: // LD SP,d16
            cpu->sp = read16(cpu, cpu->pc);
            cpu->pc += 2;
            break;
        case 0x22: // LD (HL+), A
            write8(cpu, read_hl(cpu), cpu->a);
            write_hl(cpu, read_hl(cpu) + 1);
            break;
        case 0x32: // LD (HL-), A
            write8(cpu, read_hl(cpu), cpu->a);
            write_hl(cpu, read_hl(cpu) - 1);
            break;
        case 0xc0: // RET NZ
            if (!flag_isset(cpu, FLAG_ZERO)) {
                cpu->pc = pop(cpu);
            }
            break;
        case 0xc9: // RET
            cpu->pc = pop(cpu);
            break;
        case 0xcd: // CALL a16
            temp16 = read16(cpu, cpu->pc);
            cpu->pc += 2;
            push(cpu, cpu->pc);
            cpu->pc = temp16;
            break;
        case 0xc3: // JP a16
            cpu->pc = read16(cpu, cpu->pc);
            break;
        case 0xd2: // JP NC,a16
            if (flag_isset(cpu, FLAG_CARRY)) {
                cpu->pc = read16(cpu, cpu->pc);
            }
            break;

        case 0x80: add(cpu, cpu->b, 0); break;
        case 0x81: add(cpu, cpu->c, 0); break;
        case 0x82: add(cpu, cpu->d, 0); break;
        case 0x83: add(cpu, cpu->e, 0); break;
        case 0x84: add(cpu, cpu->h, 0); break;
        case 0x85: add(cpu, cpu->l, 0); break;
        case 0x86: add(cpu, read8(cpu, read_hl(cpu)), 0); break;
        case 0x87: add(cpu, cpu->a, 0); break;
        case 0x88: add(cpu, cpu->b, 1); break;
        case 0x89: add(cpu, cpu->c, 1); break;
        case 0x8a: add(cpu, cpu->d, 1); break;
        case 0x8b: add(cpu, cpu->e, 1); break;
        case 0x8c: add(cpu, cpu->h, 1); break;
        case 0x8d: add(cpu, cpu->l, 1); break;
        case 0x8e: add(cpu, read8(cpu, read_hl(cpu)), 1); break;
        case 0x8f: add(cpu, cpu->a, 1); break;

        case 0x90: subtract(cpu, cpu->b, 0, 0); break;
        case 0x91: subtract(cpu, cpu->c, 0, 0); break;
        case 0x92: subtract(cpu, cpu->d, 0, 0); break;
        case 0x93: subtract(cpu, cpu->e, 0, 0); break;
        case 0x94: subtract(cpu, cpu->h, 0, 0); break;
        case 0x95: subtract(cpu, cpu->l, 0, 0); break;
        case 0x96: subtract(cpu, read8(cpu, read_hl(cpu)), 0, 0); break;
        case 0x97: subtract(cpu, cpu->a, 0, 0); break;
        case 0x98: subtract(cpu, cpu->b, 1, 0); break;
        case 0x99: subtract(cpu, cpu->c, 1, 0); break;
        case 0x9a: subtract(cpu, cpu->d, 1, 0); break;
        case 0x9b: subtract(cpu, cpu->e, 1, 0); break;
        case 0x9c: subtract(cpu, cpu->h, 1, 0); break;
        case 0x9d: subtract(cpu, cpu->l, 1, 0); break;
        case 0x9e: subtract(cpu, read8(cpu, read_hl(cpu)), 1, 0); break;
        case 0x9f: subtract(cpu, cpu->a, 1, 0); break;

        // AND
        case 0xa0: and(cpu, cpu->b); break;
        case 0xa1: and(cpu, cpu->c); break;
        case 0xa2: and(cpu, cpu->d); break;
        case 0xa3: and(cpu, cpu->e); break;
        case 0xa4: and(cpu, cpu->h); break;
        case 0xa5: and(cpu, cpu->l); break;
        case 0xa6: and(cpu, read8(cpu, read_hl(cpu))); break;
        case 0xa7: and(cpu, cpu->a); break;
        case 0xe6: and(cpu, read8(cpu, cpu->pc)); cpu->pc++; break;

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

        // OR
        case 0xb0: or(cpu, cpu->b); break;
        case 0xb1: or(cpu, cpu->c); break;
        case 0xb2: or(cpu, cpu->d); break;
        case 0xb3: or(cpu, cpu->e); break;
        case 0xb4: or(cpu, cpu->h); break;
        case 0xb5: or(cpu, cpu->l); break;
        case 0xb6: or(cpu, read8(cpu, read_hl(cpu))); break;
        case 0xb7: or(cpu, cpu->a); break;

        // CP
        case 0xb8: subtract(cpu, cpu->b, 0, 1); break;
        case 0xb9: subtract(cpu, cpu->c, 0, 1); break;
        case 0xba: subtract(cpu, cpu->d, 0, 1); break;
        case 0xbb: subtract(cpu, cpu->e, 0, 1); break;
        case 0xbc: subtract(cpu, cpu->h, 0, 1); break;
        case 0xbd: subtract(cpu, cpu->l, 0, 1); break;
        case 0xbe: subtract(cpu, read8(cpu, read_hl(cpu)), 0, 1); break;
        case 0xbf: subtract(cpu, cpu->a, 0, 1); break;
        case 0xfe: subtract(cpu, read8(cpu, cpu->pc), 0, 1); cpu->pc++; break;

        // RST
        case 0xc7: push(cpu, cpu->pc); cpu->pc = 0x00; break;
        case 0xcf: push(cpu, cpu->pc); cpu->pc = 0x08; break;
        case 0xd7: push(cpu, cpu->pc); cpu->pc = 0x10; break;
        case 0xdf: push(cpu, cpu->pc); cpu->pc = 0x18; break;
        case 0xe7: push(cpu, cpu->pc); cpu->pc = 0x20; break;
        case 0xef: push(cpu, cpu->pc); cpu->pc = 0x28; break;
        case 0xf7: push(cpu, cpu->pc); cpu->pc = 0x30; break;
        case 0xff: push(cpu, cpu->pc); cpu->pc = 0x38; break;

        case 0xc1: // POP BC
            write_bc(cpu, pop(cpu));
            break;
        case 0xc5: // PUSH BC
            push(cpu, read_bc(cpu));
            break;
        case 0xcb:
            extended_insn(cpu, read8(cpu, cpu->pc));
            cpu->pc++;
            break;
        case 0xce:
            add(cpu, read8(cpu, cpu->pc), 1);
            cpu->pc++;
            break;
        case 0xe0: // LD (a8),A
            write8(cpu, 0xff00 + read8(cpu, cpu->pc), cpu->a);
            cpu->pc++;
            break;
        case 0xe2: // LD (C),A
            write8(cpu, 0xff00 + cpu->c, cpu->a);
            break;
        case 0xea: // LD (a16),A
            write8(cpu, read16(cpu, cpu->pc), cpu->a);
            cpu->pc += 2;
            break;
        case 0xf0: // LD A,(a8)
            cpu->a = read8(cpu, 0xff00 + read8(cpu, cpu->pc));
            cpu->pc++;
            break;
        case 0xf2: // LD A,(C)
            cpu->a = read8(cpu, 0xff00 + cpu->c);
            break;
        case 0xf3: // DI
            break;
        case 0xfb: // EI
            break;
        default:
            printf("unknown opcode 0x%02x %s\n", opc, instructions[opc].format);
            cpu_panic(cpu);
    }
}
