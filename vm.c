#include "vm.h"
#include "common.h"
#include "debug.h"
#include  "chunk.h"
#include "memory.h"
#include "value.h"
#include "compiler.h"
#include <stdio.h>
#include <stdarg.h>



VM vm;

//resets the stackTop pointer to the initial stack pointer
static void resetStack(){
    vm.stackCount = 0;
}

static void runtimeError(const char* format, ...){
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    size_t instruction =  vm.ip-vm.chunk->code-1;
    int line = getLine(vm.chunk, instruction);
    fprintf(stderr, "[line %d] in script \n", line);

    resetStack();
}

void initVM(){
    vm.stackCapacity = 0;
    vm.stackCount = 0;
    resetStack();
}

void freeVM(){

}

void push(Value value){
    if(vm.stackCapacity < vm.stackCount + 1){
        int oldCapacity = vm.stackCapacity;
        vm.stackCapacity = GROW_CAPACITY(oldCapacity);
        vm.stack = (Value*)reallocate(vm.stack, sizeof(Value) * (oldCapacity), sizeof(Value) * (vm.stackCapacity));
    }
    vm.stack[vm.stackCount] = value;
    vm.stackCount++;
}

Value pop(){
    vm.stackCount--;
    return vm.stack[vm.stackCount];
}

static Value peek(int distance){
    Value a = vm.stack[vm.stackCount];
    Value b = vm.stack[vm.stackCount - 1 - 1];
    return vm.stack[vm.stackCount -1 - distance];
}

bool isFalsey(Value value){
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}



static InterpretResult run(){
    //vm.ip is the instruction pointer : 
        // Which byte is it about to execute?
    #define READ_BYTE() (*vm.ip++)
    #define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()]) 
    #define BINARY_OP(valueType, op) \
        do { \
            if(!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))){ \
                runtimeError("Operands must be numbers"); \
                return INTERPRET_RUNTIME_ERR;\
            }\
            double b = AS_NUM(pop()); \
            double a = AS_NUM(pop()); \
            push(valueType(a op b)); \
        } while(false) 
    
    while(true){
        #ifdef DEBUG_TRACE_EXECUTION
            printf("      ");
            for(int slot = 0; slot < vm.stackCount; slot++){
                printf("[ ");
                printf(" ]");
            }
            printf("\n");
            // vm.ip will always represent the next set of instructions,
            //So we will need to minus the 
            disassembleInstruction(vm.chunk, (int)(vm.ip-vm.chunk->code));
            
        #endif
        uint8_t instructions;
        switch(instructions = READ_BYTE()) {
            case OP_CONSTANT:{
                Value constant = READ_CONSTANT();
                push(constant);
                break;
            }
               
            case OP_RETURN: 
                printValue(pop());
                return INTERPRET_OK; 
            
            case OP_NEGATE:
                //Makes the top number negative
                if(!IS_NUMBER(peek(0))){
                    runtimeError("Operand must be a number");
                    return INTERPRET_RUNTIME_ERR;
                }
                push(NUMBER_VAL(-AS_NUM(pop())));
                break;
            case OP_ADD:
                BINARY_OP(NUMBER_VAL,+);
                break;
            case OP_SUBTRACT:
                BINARY_OP(NUMBER_VAL,-);
                break;
            case OP_MULTIPLY:
                BINARY_OP(NUMBER_VAL,*);
                break;
            case OP_DIVIDE:
                BINARY_OP(NUMBER_VAL,/);
                break;
            case OP_NIL:
                push(NIL_VAL); break;
            case OP_TRUE:
                push(BOOL_VAL(true)); break;
            case OP_FALSE:
                push(BOOL_VAL(false)); break;
            case OP_NOT:
                push(BOOL_VAL(isFalsey(pop()))); break;
            case OP_EQUAL: {
                Value b = pop();
                Value a = pop();
                push(BOOL_VAL(valuesEqual(a,b)));
                break;
            }
            case OP_GREATER:
                BINARY_OP(BOOL_VAL, >);  break;
            case OP_LESS:
                BINARY_OP(BOOL_VAL, <); break;
        }
    }
    #undef READ_BYTE 
    #undef READ_CONSTANT
}

InterpretResult interpret(const char* source){
    Chunk chunk;
    initChunk(&chunk);
    //pass chunk in for compile
    if(!compile(source, &chunk)) {
        freeChunk(&chunk);
        return INTERPRET_COMPILE_ERR;
    }
    vm.chunk = &chunk;
    vm.ip = vm.chunk->code; 

    InterpretResult result = run();

    freeChunk(&chunk);
    return result;
}