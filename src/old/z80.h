/* Game Boy emulator for 68k Macs
   Compiled with Symantec THINK C 5.0
   (c) 2013 Matt Laux
   
   z80.h - definitions and prototypes for z80.c */
   
#ifndef Z80_H
#define Z80_H
   
#define GB_RAM_SIZE 0x2000

#define FLAG_ZERO 0x80
#define FLAG_SUBTRACT 0x40
#define FLAG_HALF_CARRY 0x20
#define FLAG_CARRY 0x10

typedef struct _z80_regs {
	union {
		struct {
			u8 f;
			u8 a;
		} ind;
		u16 val;
	} af;
	
	union {
		struct {
			u8 c;
			u8 b;
		} ind;
		u16 val;
	} bc;
	
	union {
		struct {
			u8 e;
			u8 d;
		} ind;
		u16 val;
	} de;
	
	union {
		struct {
			u8 l;
			u8 h;
		} ind;
		u16 val;
	} hl;
	
	u16 sp;
	u16 pc;
} z80_regs;

typedef struct _z80_state {
	z80_regs *regs;
	u8 current_op;
	u16 current_pc;
	u8 *ram;
} z80_state;

z80_state *z80_create(void);
void z80_destroy(z80_state *state);
void z80_dump_regs(z80_state *state);
void z80_run(z80_state *state);

#endif