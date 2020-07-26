#include "common.h"
#include "chunk.c"
#include "debug.h"
#include "memory.c"
#include <stdio.h>
#include "debug.c"
#include "value.c"
#include "value.h"

int main(int argc, const char* argv[]) {
    Chunk chunk;
    initChunk(&chunk);
    int constant= addConstant(&chunk, 1.2);
    writeChunk(&chunk, OP_CONSTANT, 123);
    writeChunk(&chunk, constant, 123);
    // writeChunk(&chunk, OP_RETURN);
    disassembleChunk(&chunk, "jeff");
    freeChunk(&chunk);


    return 0;
}