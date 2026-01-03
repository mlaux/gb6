#ifndef _DEBUG_H
#define _DEBUG_H

#include "compiler.h"

void debug_log_string(const char *str);

void debug_log_block(struct code_block *block);

#endif