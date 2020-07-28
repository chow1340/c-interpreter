#include <stdio.h>
#include "debug.h"
#include "value.h"

//let disassembleInstructions() handle incrementing to return offset of next instruction
void disassembleChunk(Chunk* chunk, const char* name){
    // %s takes next arguments and prints as string
    printf("== %s ==\n", name);
    for(int offset = 0; offset< chunk->count;){
        offset = disassembleInstruction(chunk, offset);
    }
}
static int simpleInstruction(const char* name, int offset){
    printf("%s\n ", name);
    return offset + 1;
}

static int constantInstruction(const char* name, Chunk* chunk, int offset){
    uint8_t constant = chunk->code[offset + 1];
    printf("%-16s %4d'" , name, constant);
    printValue(chunk->constants.values[constant]);
    printf("'\n");
    return offset + 2;
}

static int longConstantInstruction(const char* name, Chunk* chunk, int offset){
    uint32_t constant = chunk->code[offset+1] |  
                        (chunk->code[offset+2] << 8) |
                        (chunk->code[offset+3] << 16);

    printf("%-16s %4d '",  name,  constant);
    printValue(chunk->constants.values[constant]);
    printf("'\n");
    return offset+4;
}
int disassembleInstruction(Chunk* chunk, int offset){
    printf("%08d ", offset);
    int line = getLine(chunk, offset);
    if(offset > 0 && line == getLine(chunk, offset-1)) {
        printf(" |");
    } else {
        printf("%4d", line);
    }
    uint8_t instruction = chunk->code[offset];
    switch(instruction){
        case OP_RETURN:
            return simpleInstruction("OP_RETURN", offset);

        case OP_CONSTANT:
            return constantInstruction("OP_CONSTANT", chunk, offset);
        
        case OP_CONSTANT_LONG:
            return longConstantInstruction("OP_CONSTANT_LONG", chunk, offset);
        default:
            printf("Unknown opcode %d\n", instruction);
            return offset + 1;
    }
}

