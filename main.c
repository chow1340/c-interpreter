#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "vm.h"
static void repl(){
    char line[1024];
    while(true){
        printf("> ");
        if(!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }
        interpret(line);
    }
}

//Goal is to allocate a string large enough to read the file
static char* readFile(const char* path){
    FILE* file = fopen(path, "r");
    if(file == NULL){
        fprintf(stderr, "Could not open file \"%s\".\n", path);
        exit(74);
    }
    fseek(file, 0L, SEEK_END); //Seek the end of the file
    size_t fileSize = ftell(file); //Get bytes we are from size of file
    rewind(file); //Rewind back to beginning

    //Allocate size of string to buffer
    char* buffer = (char*)malloc(fileSize+1);
    if(buffer == NULL){
        fprintf(stderr, "Not enough memory to read \"%s\" . \n", path);
        exit(74);
    }

    //Read through file
    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
    if(bytesRead < fileSize) {
        fprintf(stderr, "Could not read file \"%s\" . \n", path);
        exit(74);
    }
    buffer[bytesRead] = '\0';

    fclose(file);
    return buffer;
}

//Read the file and execute the string of source code
static void runFile(const char* path) {
    char* source = readFile(path);
    InterpretResult result = interpret(source);
    free(source);

    if(result == INTERPRET_COMPILE_ERR) exit(65);
    if(result == INTERPRET_RUNTIME_ERR) exit(70);
}

int main(int argc, const char* argv[]) {
    initVM();
    if(argc == 1) {
        repl();
    } else if (argc == 2){
        runFile(argv[1]);
    } else {
        fprintf(stderr, "Usage: cInterp [path]\n");
        exit(64);
    }
    
    freeVM();
    // freeChunk(&chunk);
    return 0;
}