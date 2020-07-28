#include "common.h"
#include "chunk.c"
#include "debug.h"
#include "memory.c"
#include <stdio.h>
#include "debug.c"
#include "value.c"
#include "value.h"
#include "value.h"
#include "vm.c"
#include "vm.h"

int main(int argc, const char* argv[]) {
    initVM();
    Chunk chunk;
    initChunk(&chunk);
    int constant= addConstant(&chunk, 1);
    writeChunk(&chunk, OP_CONSTANT, 123);
    writeChunk(&chunk, constant, 123);
     int constant2= addConstant(&chunk, 2);
    writeChunk(&chunk, OP_CONSTANT, 123);
    writeChunk(&chunk, constant2, 123);
     
    interpret(&chunk);
    
    // disassembleChunk(&chunk, "jeff");

    freeVM();
    freeChunk(&chunk);
 
    return 0;
}