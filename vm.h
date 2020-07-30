#ifndef cInterp_vm_h
#define cInterp_vm_h

#include "common.h"
#include "chunk.h"
#include "value.h"



typedef struct{
    Chunk* chunk;
    u_int8_t* ip;
    Value* stack;
    int stackCount;
    int stackCapacity;
} VM;

typedef enum{
    INTERPRET_OK,
    INTERPRET_RUNTIME_ERR,
    INTERPRET_COMPILE_ERR,
} InterpretResult;

void initVM();
void freeVM();
void push();
Value pop();
InterpretResult interpret(const char* source);

#endif
