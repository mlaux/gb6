#include "gb_types.h"
#include "z80.h"
#include "emulator.h"
#include "mem_model.h"

u8 mem_read(u16 addr)
{
	if(addr >= 0x0000 && addr <= 0x7fff)
		return theState.rom[addr];
	//else if(addr >= 
}

u16 mem_read_word(u16 addr)
{
	if(addr >= 0x0000 && addr <= 0x7fff)
		return theState.rom[addr] | theState.rom[addr + 1] << 8;
}

void mem_write(u16 addr, u8 value)
{

}

void mem_write_word(u16 addr, u16 value)
{

}