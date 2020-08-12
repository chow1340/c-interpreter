#ifndef cInterp_compiler_h
#define cInterp_compiler_h

#include "vm.h"
#include "scanner.h"
#include "object.h"

ObjFunction* compile(const char* source);

#endif