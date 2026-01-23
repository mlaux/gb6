#ifndef _CPU_CACHE_H
#define _CPU_CACHE_H

#include <OSUtils.h>

#define _Unimplemented 0xa89f
#define _CacheFlush 0xa0bd

Boolean TrapAvailable(short trapWord);

#endif