#ifndef cInterp_compiler_h
#define cInterp_compiler_h

#include "vm.h"
#include "scanner.h"


bool compile(const char* source, Chunk* chunk);

#endif