#ifndef cInterp_chunk_h
#define cInterp_chunk_h

#include "common.h"
#include "value.h"
// One-byte operation code
typedef enum{
    OP_RETURN,
    OP_CONSTANT,
    OP_CONSTANT_LONG,
} OpCode; 

typedef struct{
    int offset;
    int line;

} LineStart;

typedef struct{
    int count;
    int capacity;
    uint8_t* code;
    ValueArray constants;
    int lineCount;
    int lineCapacity;
    LineStart* lines;
} Chunk;


void initChunk(Chunk* chunk);
void freeChunk(Chunk* chunk);
void writeChunk(Chunk* chunk, uint8_t byte, int line);
int addConstant(Chunk* chunk, Value value);
void writeConstant(Chunk* chunk, Value value, int line);
int getLine(Chunk* chunk, int instruction);
#endif

