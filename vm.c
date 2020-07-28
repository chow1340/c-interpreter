#include "vm.h"
#include "common.h"
#include "debug.h"
#include  "chunk.h"

VM vm;

//resets the stackTop pointer to the initial stack pointer
static void resetStack(){
    vm.stackTop = vm.stack;
}

void initVM(){
    resetStack();
}

void freeVM(){

}

void push(Value value){
    *vm.stackTop = value;
    vm.stackTop++;
}

Value pop(){
    vm.stackTop--;
    return *vm.stackTop;
}

static InterpretResult run(){
    //vm.ip is the instruction pointer : 
        // Which byte is it about to execute?
    #define READ_BYTE() (*vm.ip++)
    #define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()]) 
    while(true){
        #ifdef DEBUG_TRACE_EXECUTION
            printf("      ");
            for(Value* slot = vm.stack; slot < vm.stackTop; slot++){
                printf("[ ");
                printValue(*slot);
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
                return INTERPRET_OK; 
        }
    }
    #undef READ_BYTE 
    #undef READ_CONSTANT
}

InterpretResult interpret(Chunk* chunk){
    vm.chunk = chunk;
    vm.ip = vm.chunk->code;
    return run();
}