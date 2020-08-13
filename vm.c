#include "vm.h"
#include "common.h"
#include "debug.h"
#include "chunk.h"
#include "memory.h"
#include "value.h"
#include "compiler.h"
#include "object.h"
#include "table.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>


VM vm;

Obj* objects;

static void resetStack(){
    vm.stackCount = 0;
}
static Value clockNative(int argCount, Value* args){
    return NUMBER_VAL((double)clock()/CLOCKS_PER_SEC);
}

static void runtimeError(const char* format, ...){
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    // CallFrame* frame = &vm.frames[vm.frameCount - 1];
    // size_t instruction = frame->ip - frame->function->chunk.code - 1;
    // int line = getLine(&frame->function->chunk, instruction);
    // fprintf(stderr, "[line %d] in script \n", line);

    for(int i = vm.frameCount - 1; i >= 0; i--) {
        CallFrame* frame = &vm.frames[i];
        ObjFunction* function = frame->closure->function;

        //-1 cause IP is sitting on the next instruction to be executed
        size_t instruction = frame->ip - frame->closure->function->chunk.code - 1;
        fprintf(stderr, "[line %d] in", function->chunk.lines[instruction].line);
        if(function->name == NULL){
            fprintf(stderr, "script \n");
        } else {
            fprintf(stderr, "%s()\n", function->name->chars);
        }
    }

    resetStack();
}

static void defineNative(const char* name, NativeFn function) {
    push(OBJ_VAL(copyString(name, (int)strlen(name))));
    push(OBJ_VAL(newNative(function)));
    tableSet(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
    pop();
    pop();
}

void initVM(){
    vm.stackCapacity = 0;
    vm.stackCount = 0;
    vm.objects = NULL;
    vm.frameCount = 0;
    initTable(&vm.globals);
    initTable(&vm.strings);
    defineNative("clock", clockNative);
    resetStack();
}

void freeVM(){
    freeTable(&vm.strings);
    freeTable(&vm.globals);
    freeObjects();
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
    return vm.stack[vm.stackCount -1 - distance];
}

bool isFalsey(Value value){
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static bool call(ObjClosure* closure, int argCount) {
    if(argCount != closure->function->arity) {
        runtimeError("Expect %d arguments but got %d.", closure->function->arity, argCount);
        return false;
    }
    if(vm.frameCount == FRAMES_MAX) {
        runtimeError("Stack overflow.");
        return false;
    }
    //Initialize frame
    CallFrame* frame = &vm.frames[vm.frameCount++]; 
    frame->closure = closure;
    frame->ip = closure->function->chunk.code;
    frame->slots = &vm.stack[vm.stackCount - argCount - 1];
    frame->start = vm.stackCount - argCount - 1;
    return true;
}

static bool callValue(Value callee, int argCount) {
    if(IS_OBJ(callee)) {
        switch(OBJ_TYPE(callee)) {
            // case OBJ_FUNCTION:
            //     return call(AS_FUNCTION(callee), argCount);
            case OBJ_CLOSURE: {
                return call(AS_CLOSURE(callee), argCount);
            }
            case OBJ_NATIVE: {
                NativeFn native = AS_NATIVE(callee);
                Value result = native(argCount, vm.stack - argCount);
                vm.stackCount -= argCount + 1;
                push(result);
                return true;
            }
            default:
                break;
        }
    }
    runtimeError("Can only call functions and classes");
    return false;
}

static void concatenate(){
    ObjString* bString = AS_STRING(pop());
    ObjString* aString = AS_STRING(pop());

    int length = aString->length + bString->length;
    char* chars = ALLOCATE(char, length +1);
    memcpy(chars, aString->chars, aString->length);
    memcpy(chars + aString->length, bString->chars, bString->length);
    chars[length] = '\0';
    
    ObjString* result = takeString(chars, length);
    push(OBJ_VAL(result));
}


static InterpretResult run(){
    CallFrame* frame = &vm.frames[vm.frameCount - 1];
    //frame->ip is the instruction pointer : 
        // Which byte is it about to execute?
    #define READ_BYTE() (*frame->ip++)
    #define READ_CONSTANT() (frame->closure->function->chunk.constants.values[READ_BYTE()]) 
    #define READ_STRING() AS_STRING(READ_CONSTANT())
    //Takes the next two bytes from the chunk and build a 16 bit unsigned int
    #define READ_SHORT() \
        (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
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
            disassembleInstruction(&frame->closure->function->chunk, (int)(frame->ip - frame->closure->function->chunk.code));
            
        #endif
        uint8_t instructions;

        switch(instructions = READ_BYTE()) {
            case OP_CONSTANT:{
                Value constant = READ_CONSTANT();
                push(constant);
                break;
            }
               
            case OP_RETURN: {
                Value result = pop();
                vm.frameCount--;
                //Last frame
                if(vm.frameCount == 0) {
                    pop();
                    return INTERPRET_OK;
                }
                vm.stackCount = frame->start;
                push(result);
                frame = &vm.frames[vm.frameCount-1];
                break;      
            }
            
            case OP_NEGATE:
                //Makes the top number negative
                if(!IS_NUMBER(peek(0))){
                    runtimeError("Operand must be a number");
                    return INTERPRET_RUNTIME_ERR;
                }
                push(NUMBER_VAL(-AS_NUM(pop())));
                break;
            case OP_ADD:
                if(IS_STRING(peek(0)) && IS_STRING(peek(1))) {
                    concatenate();
                } else {
                    BINARY_OP(NUMBER_VAL,+);
                }
              
                break;
            case OP_INCREMENT:{
                if(!IS_NUMBER(peek(0))){
                    runtimeError("Value must be number");
                    return INTERPRET_RUNTIME_ERR;
                }
                push(NUMBER_VAL(AS_NUM(peek(0)) + 1));
                break;
            }
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
            case OP_PRINT:
                printValue(pop());
                printf("\n");
                break;
            case OP_POP: 
                pop();
                break;
            case OP_DEFINE_GLOBAL: {
                ObjString* name = READ_STRING();
                tableSet(&vm.globals, name, peek(0));
                pop();
                break;
            }
            case OP_GET_GLOBAL:{
                ObjString* name = READ_STRING();
                Value value;
                if(!tableGet(&vm.globals,  name, &value)) {
                    runtimeError("Undefined variable '%s' .",  name->chars);
                    return INTERPRET_RUNTIME_ERR;
                }
                push(value);
                break;
            }
            
            case OP_SET_GLOBAL:{
                ObjString* name = READ_STRING();
                if(tableSet(&vm.globals, name, peek(0))) {
                    tableDelete(&vm.globals, name);
                    runtimeError("Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERR;
                }
                break;
            }
            //Pushes the local to top
            case OP_GET_LOCAL:{
                uint8_t slot = READ_BYTE();
                push(frame->slots[slot]);
                break;
            }

            case OP_SET_LOCAL:{
                //Sets assigned value from the top and stores in the stack slot
                //corresponding with the local variable
                uint8_t slot = READ_BYTE();
                frame->slots[slot] = peek(0);
                break;
            }

            case OP_JUMP_IF_FALSE: {
                uint16_t offset = READ_SHORT();
                if(isFalsey(peek(0))) frame->ip += offset;
                break;
            }
            case OP_JUMP: {
                uint16_t offset = READ_SHORT();
                frame->ip += offset;
                break;
            }
            case OP_LOOP: {
                uint16_t offset = READ_SHORT();
                frame->ip -=offset; //Sends pointer back to begin of loop
                break;
            }
            case OP_CALL: {
                int argCount = READ_BYTE();
                if(!callValue(peek(argCount), argCount)){
                    return INTERPRET_RUNTIME_ERR;
                }
                frame = &vm.frames[vm.frameCount - 1];
                break;
            }
            case OP_CLOSURE:{
                ObjFunction* function = AS_FUNCTION(READ_CONSTANT());
                ObjClosure* closure = newClosure(function);
                push(OBJ_VAL(closure));
                break;
            }
        }
    }
    #undef READ_BYTE 
    #undef READ_STRING
    #undef READ_SHORT
    #undef READ_CONSTANT
}

InterpretResult interpret(const char* source){
    ObjFunction* function = compile(source);
    if(function == NULL) return INTERPRET_COMPILE_ERR;
    push(OBJ_VAL(function));
    //Initialize callframe for script
    ObjClosure* closure = newClosure(function);
    pop();
    push(OBJ_VAL(closure));
    callValue(pop(), 0);
    return run();
}

