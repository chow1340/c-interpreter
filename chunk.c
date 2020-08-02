#include "chunk.h"
#include <stdlib.h>
#include "memory.h"

// Intialize empty new chunk
void initChunk(Chunk* chunk){
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    chunk->lineCount = 0;
    chunk->lineCapacity = 0;
    chunk->lines = NULL;
    initValueArray(&chunk->constants);
}   

void writeChunk(Chunk* chunk, uint8_t byte, int line){
    //Reassign capacity if needed
    if(chunk->capacity < chunk->count +1) {
        int oldCapacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(oldCapacity);
        chunk->code = GROW_ARRAY(uint8_t, chunk->code, oldCapacity, chunk->capacity);
    }
 
    chunk->code[chunk->count] = byte;
    chunk->count++;
    
    //see if same line
    if(chunk->lineCount > 0 && chunk->lines[chunk->lineCount - 1].line == line) {
        return;
    }

    //expand linecapacity if necessary
    if(chunk->lineCapacity < chunk->lineCount + 1){
        int oldCapacity = chunk->lineCapacity;
        chunk->lineCapacity = GROW_CAPACITY(oldCapacity);
        chunk->lines = GROW_ARRAY(LineStart,chunk->lines , oldCapacity, chunk->lineCapacity);
    }
    //Update lines struct for chunk
    //Offset marks the first byte in that line, 
    //any bytes after that are considered the same line
    LineStart* lineStart = &chunk->lines[chunk->lineCount]; 
    chunk->lineCount++;
    lineStart->offset = chunk->count - 1; 
    lineStart->line = line;
}

void freeChunk(Chunk* chunk) {
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
    FREE_ARRAY(int, chunk->lines, chunk->lineCapacity);
    freeValueArray(&chunk->constants);
    initChunk(chunk);
}

int addConstant(Chunk* chunk, Value value){
    writeValueArray(&chunk->constants, value);
    return chunk->constants.count - 1;
}

void writeConstant(Chunk* chunk, Value value, int line){
    int index = addConstant(chunk, value);
    //if 1 byte, use the OP_CONSTANT    
    if(index < 256){
        writeChunk(chunk, OP_CONSTANT, line);
        writeChunk(chunk, index, line);
    }else{
        //Uses little endian to break the 24 bit constant
        writeChunk(chunk, OP_CONSTANT_LONG, line);
        writeChunk(chunk,(uint8_t)(index & 0xff), line);
        writeChunk(chunk, (u_int8_t)(index >> 8 & 0xff), line);
        writeChunk(chunk, (u_int8_t)(index >> 16 & 0xff), line);
    }
}
int getLine(Chunk* chunk, int instruction){
    int start = 0;
    int end = chunk->lineCount - 1;

    while(true){
        int mid = (start+end)/2;
        LineStart* line = &chunk->lines[mid];
        if(instruction < line->offset){
            end = mid-1;
        } else if (mid == chunk->lineCount - 1 || instruction < chunk->lines[mid+1].offset) {
            return line->line;
        } else {
            start = mid+1;
        }
     
    }
}