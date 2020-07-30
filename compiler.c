#include <stdio.h>
#include "scanner.h"
#include "common.h"
#include "compiler.h"
#include "scanner.c"

void compile(const char* source){
    initScanner(source);
    int line = -1;
    while(true){
        Token token = scanToken();
        if(token.line != line){
            printf("%4d ", token.line);
            line = token.line;
        } else {
            printf("    | ");
        }
        //prints beginning to end of token
        printf("%2d '%.*s' \n", token.type, token.length, token.start);

        if(token.type == TOKEN_EOF) break;
    }
}