#ifndef MEM_MODEL_H
#define MEM_MODEL_H

u8 mem_read(u16 addr);
u16 mem_read_word(u16 addr);
void mem_write(u16 addr, u8 value);
void mem_write_word(u16 addr, u16 value);

#endif