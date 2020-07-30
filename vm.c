#include "vm.h"
#include "common.h"
#include "debug.h"
#include  "chunk.h"
#include "memory.h"
#include "compiler.h"
#include "compiler.c"
VM vm;

//resets the stackTop pointer to the initial stack pointer
static void resetStack(){
    vm.stackCount = 0;
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

static InterpretResult run(){
    //vm.ip is the instruction pointer : 
        // Which byte is it about to execute?
    #define READ_BYTE() (*vm.ip++)
    #define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()]) 
    #define BINARY_OP(op) \
        do { \
            double b = pop(); \
            double a = pop(); \
            push(a op b); \
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
                push(-pop());
                break;
            case OP_ADD:
                BINARY_OP(+);
                break;
            case OP_SUBTRACT:
                BINARY_OP(-);
                break;
            case OP_MULTIPLY:
                BINARY_OP(*);
                break;
            case OP_DIVIDE:
                BINARY_OP(/);
                break;
        }
    }
    #undef READ_BYTE 
    #undef READ_CONSTANT
}

InterpretResult interpret(const char* source){
    compile(source);
    return INTERPRET_OK;
}