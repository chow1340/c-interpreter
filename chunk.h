#ifndef cInterp_chunk_h
#define cInterp_chunk_h

#include "common.h"
#include "value.h"
// One-byte operation code
typedef enum{
    OP_RETURN,
    OP_CONSTANT
} OpCode; 

typedef struct{
    int count;
    int capacity;
    uint8_t* code;
    int* lines;
    ValueArray constants;
} Chunk;

void initChunk(Chunk* chunk);
void freeChunk(Chunk* chunk);
void writeChunk(Chunk* chunk, uint8_t byte, int line);
int addConstant(Chunk* chunk, Value value);
#endif

