#include <stdio.h>
#include <stdlib.h>

#include "cpu.h"
#include "dmg.h"
#include "types.h"
#include "instructions.h"

// fast opcode/operand fetch: direct page table access for ROM (0x0000-0x7fff),
// fall back to dmg_read_slow for code executed from HRAM
#define FAST_ROM_READ(dmg, addr) \
    (((addr) < 0x8000) \
        ? (dmg)->read_page[(addr) >> 8][(addr) & 0xff] \
        : dmg_read_slow((dmg), (addr)))

// fast memory access using page table, with fallback to dmg_read/write
// for addresses without page table entries (I/O, HRAM, unmapped external RAM)
#define FAST_READ(dmg, addr) \
    ((dmg)->read_page[(addr) >> 8] \
        ? (dmg)->read_page[(addr) >> 8][(addr) & 0xff] \
        : dmg_read_slow((dmg), (addr)))

#define FAST_WRITE(dmg, addr, val) \
    do { \
        u8 *_page = (dmg)->write_page[(addr) >> 8]; \
        if (_page) _page[(addr) & 0xff] = (val); \
        else dmg_write_slow((dmg), (addr), (val)); \
    } while (0)

// non-static for debug output in imgui version
inline int flag_isset(struct cpu *cpu, int flag)
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

#define read_af(cpu) ((cpu)->af)
#define read_bc(cpu) ((cpu)->bc)
#define read_de(cpu) ((cpu)->de)
#define read_hl(cpu) ((cpu)->hl)

#define write_af(cpu, value) ((cpu)->af = (value))
#define write_bc(cpu, value) ((cpu)->bc = (value))
#define write_de(cpu, value) ((cpu)->de = (value))
#define write_hl(cpu, value) ((cpu)->hl = (value))

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

#ifdef UNITY_BUILD
static inline __attribute__((always_inline))
#else
static inline
#endif
u8 read8(struct cpu *cpu, u16 address)
{
    return FAST_READ(cpu->dmg, address);
}

// optimized read for PC-based operand fetches (always in ROM when PC is in ROM)

#ifdef UNITY_BUILD
static inline __attribute__((always_inline))
#else
static inline
#endif
u8 read8_pc(struct cpu *cpu)
{
    u16 addr = cpu->pc++;
    return FAST_ROM_READ(cpu->dmg, addr);
}

#ifdef UNITY_BUILD
static inline __attribute__((always_inline))
#else
static inline
#endif
u16 read16_pc(struct cpu *cpu)
{
    u16 addr = cpu->pc;
    cpu->pc += 2;
    u8 low = FAST_ROM_READ(cpu->dmg, addr);
    u8 high = FAST_ROM_READ(cpu->dmg, addr + 1);
    return high << 8 | low;
}

#ifdef UNITY_BUILD
static inline __attribute__((always_inline))
#else
static inline
#endif
u16 read16(struct cpu *cpu, u16 address)
{
    u8 low = read8(cpu, address);
    u8 high = read8(cpu, address + 1);
    return high << 8 | low;
}

#ifdef UNITY_BUILD
static inline __attribute__((always_inline))
#else
static inline
#endif
void write8(struct cpu *cpu, u16 address, u8 data)
{
    FAST_WRITE(cpu->dmg, address, data);
}

#ifdef UNITY_BUILD
static inline __attribute__((always_inline))
#else
static inline
#endif
void write16(struct cpu *cpu, u16 address, u16 data)
{
    FAST_WRITE(cpu->dmg, address, data & 0xff);
    FAST_WRITE(cpu->dmg, address + 1, data >> 8);
}

static void inc_with_carry(struct cpu *cpu, u8 *reg)
{
	int half = (*reg & 0xf) == 0xf;
	(*reg)++;
	cpu->f = (cpu->f & FLAG_CARRY) | (*reg ? 0 : FLAG_ZERO) | (half ? FLAG_HALF_CARRY : 0);
}

static void dec_with_carry(struct cpu *cpu, u8 *reg)
{
	int half = (*reg & 0xf) == 0;
	(*reg)--;
	cpu->f = (cpu->f & FLAG_CARRY) | (*reg ? 0 : FLAG_ZERO) | FLAG_SIGN | (half ? FLAG_HALF_CARRY : 0);
}

static u8 rotate_left(struct cpu *cpu, u8 val)
{
	int old_carry = flag_isset(cpu, FLAG_CARRY);
	u8 result = (val << 1) | old_carry;
	cpu->f = (result ? 0 : FLAG_ZERO) | ((val & 0x80) ? FLAG_CARRY : 0);
	return result;
}

static u8 rlc(struct cpu *cpu, u8 val)
{
	int old_msb = (val & 0x80) >> 7;
	u8 result = (val << 1) | old_msb;
	cpu->f = (result ? 0 : FLAG_ZERO) | (old_msb ? FLAG_CARRY : 0);
	return result;
}

static u8 rotate_right(struct cpu *cpu, u8 val)
{
	int old_carry = flag_isset(cpu, FLAG_CARRY);
	u8 result = (old_carry << 7) | (val >> 1);
	cpu->f = (result ? 0 : FLAG_ZERO) | ((val & 0x01) ? FLAG_CARRY : 0);
	return result;
}

static u8 rrc(struct cpu *cpu, u8 val)
{
	int old_lsb = val & 1;
	u8 result = (old_lsb << 7) | (val >> 1);
	cpu->f = (result ? 0 : FLAG_ZERO) | (old_lsb ? FLAG_CARRY : 0);
	return result;
}

static u8 shift_left(struct cpu *cpu, u8 value)
{
	u8 result = value << 1;
	cpu->f = (result ? 0 : FLAG_ZERO) | ((value & 0x80) ? FLAG_CARRY : 0);
	return result;
}

static u8 shift_right(struct cpu *cpu, u8 value)
{
	u8 result = (signed char) value >> 1;
	cpu->f = (result ? 0 : FLAG_ZERO) | ((value & 0x01) ? FLAG_CARRY : 0);
	return result;
}

static u8 srl(struct cpu *cpu, u8 value)
{
	u8 result = value >> 1;
	cpu->f = (result ? 0 : FLAG_ZERO) | ((value & 0x01) ? FLAG_CARRY : 0);
	return result;
}

static u8 swap(struct cpu *cpu, u8 value)
{
	u8 ret = ((value & 0xf0) >> 4) | ((value & 0x0f) << 4);
	cpu->f = ret ? 0 : FLAG_ZERO;
	return ret;
}

static void xor(struct cpu *regs, u8 value)
{
	regs->a ^= value;
	regs->f = regs->a ? 0 : FLAG_ZERO;
}

static void or(struct cpu *regs, u8 value)
{
	regs->a |= value;
	regs->f = regs->a ? 0 : FLAG_ZERO;
}

static void and(struct cpu *cpu, u8 value)
{
	cpu->a &= value;
	cpu->f = (cpu->a ? 0 : FLAG_ZERO) | FLAG_HALF_CARRY;
}

static void add(struct cpu *cpu, u8 value, int with_carry)
{
    int carry = (with_carry && flag_isset(cpu, FLAG_CARRY)) ? 1 : 0;
    int sum_full = cpu->a + value + carry;
    u8 sum_trunc = (u8) sum_full;
    if (sum_trunc == 0) {
        set_flag(cpu, FLAG_ZERO);
    } else {
        clear_flag(cpu, FLAG_ZERO);
    }
    clear_flag(cpu, FLAG_SIGN);
    if (sum_full > 0xff) {
        set_flag(cpu, FLAG_CARRY);
    } else {
        clear_flag(cpu, FLAG_CARRY);
    }
    if (((cpu->a & 0xf) + (value & 0xf) + carry) & 0x10) {
        set_flag(cpu, FLAG_HALF_CARRY);
    } else {
        clear_flag(cpu, FLAG_HALF_CARRY);
    }
    cpu->a = sum_trunc;
}

static void subtract(struct cpu *cpu, u8 value, int with_carry, int just_compare)
{
    int carry = (with_carry && flag_isset(cpu, FLAG_CARRY)) ? 1 : 0;
    int sum_full = cpu->a - value - carry;
    u8 sum_trunc = (u8) sum_full;
    if (!sum_trunc) {
        set_flag(cpu, FLAG_ZERO);
    } else {
        clear_flag(cpu, FLAG_ZERO);
    }
    set_flag(cpu, FLAG_SIGN);
    if (sum_full < 0) {
        set_flag(cpu, FLAG_CARRY);
    } else {
        clear_flag(cpu, FLAG_CARRY);
    }
    if (((cpu->a & 0xf) - (value & 0xf) - carry) & 0x10) {
        set_flag(cpu, FLAG_HALF_CARRY);
    } else {
        clear_flag(cpu, FLAG_HALF_CARRY);
    }

    if (!just_compare) {
        cpu->a = sum_trunc;
    }
}

static inline void push(struct cpu *cpu, u16 value)
{
    write8(cpu, cpu->sp - 2, value & 0xff);
    write8(cpu, cpu->sp - 1, value >> 8);
    cpu->sp -= 2;
}

static inline u16 pop(struct cpu *cpu)
{
    cpu->sp += 2;
    return read8(cpu, cpu->sp - 1) << 8 | read8(cpu, cpu->sp - 2);
}

static void add16(struct cpu *cpu, u16 src)
{
    int total = read_hl(cpu) + src; // promoted to int
    int trunc = total & 0xffff;
    clear_flag(cpu, FLAG_SIGN);
    if (total > 0xffff) {
        set_flag(cpu, FLAG_CARRY);
    } else {
        clear_flag(cpu, FLAG_CARRY);
    }
    if (((read_hl(cpu) & 0xfff) + (src & 0xfff)) & 0x1000) {
        // true if carry from bit 11 to bit 12
        set_flag(cpu, FLAG_HALF_CARRY);
    } else {
        clear_flag(cpu, FLAG_HALF_CARRY);
    }

    write_hl(cpu, trunc);
}

static void add_sp(struct cpu *cpu, u8 value)
{
    int total = cpu->sp + (signed char) value;
    clear_flag(cpu, FLAG_ZERO);
    clear_flag(cpu, FLAG_SIGN);
    if (total > 0xffff) {
        set_flag(cpu, FLAG_CARRY);
    } else {
        clear_flag(cpu, FLAG_CARRY);
    }
    cpu->sp = (u16) total;
}

static void ld_hl_sp(struct cpu *cpu, u8 value)
{
    int total = cpu->sp + (signed char) value;
    clear_flag(cpu, FLAG_ZERO);
    clear_flag(cpu, FLAG_SIGN);
    if (total > 0xffff) {
        set_flag(cpu, FLAG_CARRY);
    } else {
        clear_flag(cpu, FLAG_CARRY);
    }
    write_hl(cpu, total);
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
        srl,
    };

    // rl, sla, sra

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

static void conditional_jump(struct cpu *cpu, u8 opc, u8 neg_op, int flag) {
    s8 target = (s8) read8_pc(cpu);
    if ((opc == neg_op) ^ flag_isset(cpu, flag)) {
        cpu->pc += target;
        cpu->cycle_count += instructions[opc].cycles_branch - instructions[opc].cycles;
    }
}

static void daa(struct cpu *cpu)
{
    // https://forums.nesdev.org/viewtopic.php?t=15944
    if (!flag_isset(cpu, FLAG_SIGN)) {
        if (flag_isset(cpu, FLAG_CARRY) || cpu->a > 0x99) {
            cpu->a += 0x60;
            set_flag(cpu, FLAG_CARRY);
        }
        if (flag_isset(cpu, FLAG_HALF_CARRY) || (cpu->a & 0x0f) > 0x09) {
            cpu->a += 0x6;
        }
    } else {
        if (flag_isset(cpu, FLAG_CARRY)) {
            cpu->a -= 0x60;
        }
        if (flag_isset(cpu, FLAG_HALF_CARRY)) {
            cpu->a -= 0x6;
        }
    }

    if (cpu->a) {
        clear_flag(cpu, FLAG_ZERO);
    } else {
        set_flag(cpu, FLAG_ZERO);
    }
    clear_flag(cpu, FLAG_HALF_CARRY);
}

static void scf(struct cpu *cpu)
{
    clear_flag(cpu, FLAG_SIGN);
    clear_flag(cpu, FLAG_HALF_CARRY);
    set_flag(cpu, FLAG_CARRY);
}

static void ccf(struct cpu *cpu)
{
    clear_flag(cpu, FLAG_SIGN);
    clear_flag(cpu, FLAG_HALF_CARRY);
    if (flag_isset(cpu, FLAG_CARRY)) {
        clear_flag(cpu, FLAG_CARRY);
    } else {
        set_flag(cpu, FLAG_CARRY);
    }
}

static u16 handlers[] = { 0x40, 0x48, 0x50, 0x58, 0x60 };

static u16 check_interrupts(struct cpu *cpu)
{
    int k;

    if (!cpu->interrupt_enable) {
        return 0;
    }

    u16 enabled = cpu->dmg->interrupt_enabled;
    u16 requested = cpu->dmg->interrupt_requested;

    for (k = 0; k < NUM_INTERRUPTS; k++) {
        int check = 1 << k;
        if ((enabled & check) && (requested & check)) {
            // clear request flag for this interrupt and disable all further
            // interrupts until service routine executes EI or RETI
            dmg_write(cpu->dmg, 0xff0f, requested & ~check);
            cpu->interrupt_enable = 0;
            return handlers[k];
        }
    }

    return 0;
}

#ifdef UNITY_BUILD
static inline __attribute__((always_inline))
#endif
void cpu_step(struct cpu *cpu)
{
    u8 temp;
    u16 temp16, intr_dest;

    intr_dest = check_interrupts(cpu);
    if (intr_dest) {
        push(cpu, cpu->pc);
        cpu->halted = 0;
        cpu->pc = intr_dest;
        return;
    }

    if (cpu->halted) {
        return;
    }

    u8 opc = FAST_ROM_READ(cpu->dmg, cpu->pc);
#ifdef GB6_DEBUG
    printf("0x%04x %s\n", cpu->pc, instructions[opc].format);
#endif

    cpu->pc++;
    cpu->cycle_count += instructions[opc].cycles;
    switch (opc) {
        case 0: // NOP
            break;
        case 0x01: // LD BC, 0xNNNN
            write_bc(cpu, read16_pc(cpu));
            break;
        case 0x02: // LD (BC), A
            write8(cpu, read_bc(cpu), cpu->a);
            break;
        case 0x0f: // RRCA
            cpu->a = rrc(cpu, cpu->a);
            clear_flag(cpu, FLAG_ZERO);
            break;
        case 0x10: // STOP
            cpu->pc++;
            break;
        case 0x11: // LD DE,d16
            write_de(cpu, read16_pc(cpu));
            break;
        case 0x07: // RLCA
            cpu->a = rlc(cpu, cpu->a);
            clear_flag(cpu, FLAG_ZERO);
            break;
        case 0x08: // LD (a16),SP
            write16(cpu, read16_pc(cpu), cpu->sp);
            break;
        case 0x09: // ADD HL,BC
            add16(cpu, read_bc(cpu));
            break;
        case 0x19: // ADD HL,DE
            add16(cpu, read_de(cpu));
            break;
        case 0x17: // RLA
            cpu->a = rotate_left(cpu, cpu->a);
            clear_flag(cpu, FLAG_ZERO);
            break;
        case 0x1f: // RRA
            cpu->a = rotate_right(cpu, cpu->a);
            clear_flag(cpu, FLAG_ZERO);
            break;

        case 0x37: // SCF
            scf(cpu);
            break;
        case 0x3f: // CCF
            ccf(cpu);
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
        case 0x34: temp = read8(cpu, read_hl(cpu)); inc_with_carry(cpu, &temp); write8(cpu, read_hl(cpu), temp); break;
        case 0x35: temp = read8(cpu, read_hl(cpu)); dec_with_carry(cpu, &temp); write8(cpu, read_hl(cpu), temp); break;

        case 0x3b: cpu->sp--; break;
        case 0x3c: inc_with_carry(cpu, &cpu->a); break;
        case 0x3d: dec_with_carry(cpu, &cpu->a); break;

        // 8-bit immediate loads
        case 0x06: cpu->b = read8_pc(cpu); break;
        case 0x0e: cpu->c = read8_pc(cpu); break;
        case 0x16: cpu->d = read8_pc(cpu); break;
        case 0x1e: cpu->e = read8_pc(cpu); break;
        case 0x26: cpu->h = read8_pc(cpu); break;
        case 0x2e: cpu->l = read8_pc(cpu); break;
        case 0x36: write8(cpu, read_hl(cpu), read8_pc(cpu)); break;
        case 0x3e: cpu->a = read8_pc(cpu); break;

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

        // dest is *HL
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
        case 0x2a: temp16 = read_hl(cpu); cpu->a = read8(cpu, temp16); write_hl(cpu, temp16 + 1); break;
        case 0x3a: temp16 = read_hl(cpu); cpu->a = read8(cpu, temp16); write_hl(cpu, temp16 - 1); break;

        case 0x12: // LD (DE),A
            write8(cpu, read_de(cpu), cpu->a);
            break;
        case 0x18: // JR r8
            cpu->pc += (s8) read8_pc(cpu);
            break;
        case 0x20: // JR NZ,r8
        case 0x28: // JR Z,r8
            conditional_jump(cpu, opc, 0x20, FLAG_ZERO);
            break;
        case 0x30: // JR NC, i8
        case 0x38: // JR C, i8
            conditional_jump(cpu, opc, 0x30, FLAG_CARRY);
            break;
        case 0x21: // LD HL, d16
            write_hl(cpu, read16_pc(cpu));
            break;
        case 0x29: // ADD HL,HL
            add16(cpu, read_hl(cpu));
            break;
        case 0x2f: // CPL
            cpu->a = ~cpu->a;
            set_flag(cpu, FLAG_SIGN);
            set_flag(cpu, FLAG_HALF_CARRY);
            break;
        case 0x31: // LD SP,d16
            cpu->sp = read16_pc(cpu);
            break;
        case 0x22: // LD (HL+), A
            temp16 = read_hl(cpu);
            write8(cpu, temp16, cpu->a);
            write_hl(cpu, temp16 + 1);
            break;
        case 0x32: // LD (HL-), A
            temp16 = read_hl(cpu);
            write8(cpu, temp16, cpu->a);
            write_hl(cpu, temp16 - 1);
            break;
        case 0x39: // ADD HL, SP
            add16(cpu, cpu->sp);
            break;
        case 0xc0: // RET NZ
            if (!flag_isset(cpu, FLAG_ZERO)) {
                cpu->pc = pop(cpu);
                cpu->cycle_count += instructions[opc].cycles_branch - instructions[opc].cycles;
            }
            break;
        case 0xc2: // JP NZ, u16
            temp16 = read16_pc(cpu);
            if (!flag_isset(cpu, FLAG_ZERO)) {
                cpu->pc = temp16;
                cpu->cycle_count += instructions[opc].cycles_branch - instructions[opc].cycles;
            }
            break;
        case 0xc9: // RET
            cpu->pc = pop(cpu);
            break;
        case 0xcd: // CALL a16
            temp16 = read16_pc(cpu);
            push(cpu, cpu->pc);
            cpu->pc = temp16;
            break;
        case 0xc3: // JP a16
            cpu->pc = read16_pc(cpu);
            break;
        case 0xc4: // CALL NZ, u16
            temp16 = read16_pc(cpu);
            if (!flag_isset(cpu, FLAG_ZERO)) {
                push(cpu, cpu->pc);
                cpu->pc = temp16;
                cpu->cycle_count += instructions[opc].cycles_branch - instructions[opc].cycles;
            }
            break;
        case 0xcc: // CALL Z, u16
            temp16 = read16_pc(cpu);
            if (flag_isset(cpu, FLAG_ZERO)) {
                push(cpu, cpu->pc);
                cpu->pc = temp16;
                cpu->cycle_count += instructions[opc].cycles_branch - instructions[opc].cycles;
            }
            break;
        case 0xd4: // CALL NC, u16
            temp16 = read16_pc(cpu);
            if (!flag_isset(cpu, FLAG_CARRY)) {
                push(cpu, cpu->pc);
                cpu->pc = temp16;
                cpu->cycle_count += instructions[opc].cycles_branch - instructions[opc].cycles;
            }
            break;
        case 0xdc: // CALL C, u16
            temp16 = read16_pc(cpu);
            if (flag_isset(cpu, FLAG_CARRY)) {
                push(cpu, cpu->pc);
                cpu->pc = temp16;
                cpu->cycle_count += instructions[opc].cycles_branch - instructions[opc].cycles;
            }
            break;
        case 0xd0: // RET NC
            if (!flag_isset(cpu, FLAG_CARRY)) {
                cpu->pc = pop(cpu);
                cpu->cycle_count += instructions[opc].cycles_branch - instructions[opc].cycles;
            }
            break;
        case 0xd2: // JP NC,a16
            temp16 = read16_pc(cpu);
            if (!flag_isset(cpu, FLAG_CARRY)) {
                cpu->pc = temp16;
                cpu->cycle_count += instructions[opc].cycles_branch - instructions[opc].cycles;
            }
            break;
        case 0xd8: // RET C
            if (flag_isset(cpu, FLAG_CARRY)) {
                cpu->pc = pop(cpu);
                cpu->cycle_count += instructions[opc].cycles_branch - instructions[opc].cycles;
            }
            break;
        case 0xda: // JP C, u16
            temp16 = read16_pc(cpu);
            if (flag_isset(cpu, FLAG_CARRY)) {
                cpu->pc = temp16;
                cpu->cycle_count += instructions[opc].cycles_branch - instructions[opc].cycles;
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

        case 0xc6: // ADD A, u8
            add(cpu, read8_pc(cpu), 0);
            break;
        case 0xce: // ADC A, u8
            add(cpu, read8_pc(cpu), 1);
            break;

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

        case 0xd6: // SUB A, u8
            subtract(cpu, read8_pc(cpu), 0, 0);
            break;

        case 0xde: // SBC A, u8
            subtract(cpu, read8_pc(cpu), 1, 0);
            break;

        // AND
        case 0xa0: and(cpu, cpu->b); break;
        case 0xa1: and(cpu, cpu->c); break;
        case 0xa2: and(cpu, cpu->d); break;
        case 0xa3: and(cpu, cpu->e); break;
        case 0xa4: and(cpu, cpu->h); break;
        case 0xa5: and(cpu, cpu->l); break;
        case 0xa6: and(cpu, read8(cpu, read_hl(cpu))); break;
        case 0xa7: and(cpu, cpu->a); break;
        case 0xe6: and(cpu, read8_pc(cpu)); break;

        // XOR
        case 0xa8: xor(cpu, cpu->b); break;
        case 0xa9: xor(cpu, cpu->c); break;
        case 0xaa: xor(cpu, cpu->d); break;
        case 0xab: xor(cpu, cpu->e); break;
        case 0xac: xor(cpu, cpu->h); break;
        case 0xad: xor(cpu, cpu->l); break;
        case 0xae: xor(cpu, read8(cpu, read_hl(cpu))); break;
        case 0xaf: xor(cpu, cpu->a); break;
        case 0xee: xor(cpu, read8_pc(cpu)); break;

        // OR
        case 0xb0: or(cpu, cpu->b); break;
        case 0xb1: or(cpu, cpu->c); break;
        case 0xb2: or(cpu, cpu->d); break;
        case 0xb3: or(cpu, cpu->e); break;
        case 0xb4: or(cpu, cpu->h); break;
        case 0xb5: or(cpu, cpu->l); break;
        case 0xb6: or(cpu, read8(cpu, read_hl(cpu))); break;
        case 0xb7: or(cpu, cpu->a); break;
        case 0xf6:
            or(cpu, read8_pc(cpu));
            break;

        // CP
        case 0xb8: subtract(cpu, cpu->b, 0, 1); break;
        case 0xb9: subtract(cpu, cpu->c, 0, 1); break;
        case 0xba: subtract(cpu, cpu->d, 0, 1); break;
        case 0xbb: subtract(cpu, cpu->e, 0, 1); break;
        case 0xbc: subtract(cpu, cpu->h, 0, 1); break;
        case 0xbd: subtract(cpu, cpu->l, 0, 1); break;
        case 0xbe: subtract(cpu, read8(cpu, read_hl(cpu)), 0, 1); break;
        case 0xbf: subtract(cpu, cpu->a, 0, 1); break;
        case 0xfe: subtract(cpu, read8_pc(cpu), 0, 1); break;

        // RST
        case 0xc7: push(cpu, cpu->pc); cpu->pc = 0x00; break;
        case 0xcf: push(cpu, cpu->pc); cpu->pc = 0x08; break;
        case 0xd7: push(cpu, cpu->pc); cpu->pc = 0x10; break;
        case 0xdf: push(cpu, cpu->pc); cpu->pc = 0x18; break;
        case 0xe7: push(cpu, cpu->pc); cpu->pc = 0x20; break;
        case 0xef: push(cpu, cpu->pc); cpu->pc = 0x28; break;
        case 0xf7: push(cpu, cpu->pc); cpu->pc = 0x30; break;
        case 0xff: push(cpu, cpu->pc); cpu->pc = 0x38; break;

        case 0x27: // DAA
            daa(cpu);
            break;

        case 0x76: // HALT
            //cpu->halted = 1;
            break;

        case 0xc1: // POP BC
            write_bc(cpu, pop(cpu));
            break;
        case 0xc5: // PUSH BC
            push(cpu, read_bc(cpu));
            break;
        case 0xc8: // RET Z
            if (flag_isset(cpu, FLAG_ZERO)) {
                cpu->pc = pop(cpu);
                cpu->cycle_count += instructions[opc].cycles_branch - instructions[opc].cycles;
            }
            break;
        case 0xca: // JP Z, u16
            temp16 = read16_pc(cpu);
            if (flag_isset(cpu, FLAG_ZERO)) {
                cpu->pc = temp16;
                cpu->cycle_count += instructions[opc].cycles_branch - instructions[opc].cycles;
            }
            break;
        case 0xcb:
            extended_insn(cpu, read8_pc(cpu));
            break;
        case 0xd1: // POP DE
            write_de(cpu, pop(cpu));
            break;
        case 0xd5: // PUSH DE
            push(cpu, read_de(cpu));
            break;
        case 0xd9: // RETI
            cpu->pc = pop(cpu);
            cpu->interrupt_enable = 1;
            break;
        case 0xe0: // LD (a8),A
            write8(cpu, 0xff00 + read8_pc(cpu), cpu->a);
            break;
        case 0xe1: // POP HL
            write_hl(cpu, pop(cpu));
            break;
        case 0xe2: // LD (C),A
            write8(cpu, 0xff00 + cpu->c, cpu->a);
            break;
        case 0xe5: // PUSH HL
            push(cpu, read_hl(cpu));
            break;
        case 0xe8:
            add_sp(cpu, read8_pc(cpu));
            break;
        case 0xe9: // JP HL
            cpu->pc = read_hl(cpu);
            break;
        case 0xea: // LD (a16),A
            write8(cpu, read16_pc(cpu), cpu->a);
            break;
        case 0xf0: // LD A,(a8)
            cpu->a = read8(cpu, 0xff00 + read8_pc(cpu));
            break;
        case 0xf1: // POP AF
            write_af(cpu, pop(cpu));
            cpu->f &= 0xf0;
            break;
        case 0xf2: // LD A,(C)
            cpu->a = read8(cpu, 0xff00 + cpu->c);
            break;
        case 0xf3: // DI
            cpu->interrupt_enable = 0;
            break;
        case 0xf5: // PUSH AF
            push(cpu, read_af(cpu));
            break;
        case 0xf8: // LD HL, SP+i8
            ld_hl_sp(cpu, read8_pc(cpu));
            break;
        case 0xf9: // LD SP, HL
            cpu->sp = read_hl(cpu);
            break;
        case 0xfa: // LD A,(u16)
            cpu->a = read8(cpu, read16_pc(cpu));
            break;
        case 0xfb: // EI
            cpu->interrupt_enable = 1;
            break;
        default:
            printf("unknown opcode 0x%02x %s\n", opc, instructions[opc].format);
            cpu_panic(cpu);
    }
}
