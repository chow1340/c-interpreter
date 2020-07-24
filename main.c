#include "common.h"
#include "chunk.c"
#include "debug.h"
#include "memory.c"
#include <stdio.h>
#include "debug.c"

int main(int argc, const char* argv[]) {
    Chunk chunk;
    initChunk(&chunk);
    writeChunk(&chunk, OP_RETURN);
    disassembleChunk(&chunk, "jeff");
    freeChunk(&chunk);


    return 0;
}