#include <stdio.h>
#include <stdlib.h>

#include "cpu.h"
#include "types.h"

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

static inline void write_hl(struct cpu *cpu, int value)
{
    cpu->h = value >> 8;
    cpu->l = value & 0xff;
}

void cpu_panic(struct cpu *cpu)
{
    printf("a=%02x f=%02x b=%02x c=%02x\n", cpu->a, cpu->f, cpu->b, cpu->c);
    printf("d=%02x e=%02x h=%02x l=%02x\n", cpu->d, cpu->e, cpu->h, cpu->l);
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

void cpu_step(struct cpu *cpu)
{
    u8 opc = cpu->mem_read(cpu->mem_model, cpu->pc);
    switch (opc) {
        case 0: // NOP
            cpu->pc++;
            break;
        case 0x21: // LD HL, d16
            write_hl(cpu, read16(cpu, cpu->pc + 1));
            cpu->pc += 3;
            break;
        case 0xc3: // JP a16
            cpu->pc = read16(cpu, cpu->pc + 1);
            break;
        case 0xaf: // XOR A
            cpu->a = 0;
            cpu->pc++;
            break;
        default:
            printf("unknown opcode %02x\n", opc);
            cpu_panic(cpu);
    }
}
