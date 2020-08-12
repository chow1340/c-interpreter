#ifndef cInterp_vm_h
#define cInterp_vm_h

#include "common.h"
#include "chunk.h"
#include "value.h"
#include "object.h"
#include "compiler.h"
#include "table.h"

#define FRAMES_MAX 64
typedef struct {
    ObjFunction* function;
    uint8_t* ip;
    Value* slots; // points to first slot this function uses
    int start;
} CallFrame;
typedef struct{
    CallFrame frames[FRAMES_MAX];
    int frameCount;
    Value* stack;
    int stackCount;
    int stackCapacity;
    Table strings;
    Table globals;
    Obj* objects;
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

extern VM vm;

#endif
