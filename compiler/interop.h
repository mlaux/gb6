#ifndef _INTEROP_H
#define _INTEROP_H

#include <stdint.h>

void compile_call_dmg_write(struct code_block *block);
void compile_call_dmg_write_imm(struct code_block *block, uint8_t val);
void compile_call_dmg_read(struct code_block *block);
void compile_call_dmg_read_to_d0(struct code_block *block);
void compile_call_ei_di(struct code_block *block, int enabled);

#endif