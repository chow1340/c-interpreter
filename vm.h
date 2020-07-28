#ifndef cInterp_vm_h
#define cInterp_vm_h

#include "common.h"
#include "chunk.h"
#include "value.h"

#define STACK_MAX 256


typedef struct{
    Chunk* chunk;
    u_int8_t* ip;
    Value stack[STACK_MAX];
    Value* stackTop;
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
InterpretResult interpret(Chunk* chunk);

#endif
