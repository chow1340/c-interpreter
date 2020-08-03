#include <stdio.h>
#include <stdlib.h>
#include "common.h"
#include "scanner.h"
#include "compiler.h"
#include "chunk.h"
#include "debug.h"
#include "value.h"
#include "object.h"

typedef struct {
    Token current;
    Token previous;
    bool hadError;
    bool panicMode;
} Parser;

Parser parser;
//C gives increasing number for enums
//Therefore, these are ordered from lowest prec to highest
//ie if have code -a.b + c
//calling parsePrecedence(PREC_ASSIGNMENT) will parse whole expression
//calling parsePrecedence(PREC_UNARY) will only parse -a.b
typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT, //=
    PREC_OR, 
    PREC_AND,
    PREC_EQUALITY, // ==
    PREC_COMPARISON, // < > <= >=
    PREC_TERM, // + -
    PREC_FACTOR, // * /
    PREC_UNARY, // ! -
    PREC_CALL, // . ()
    PREC_PRIMARY,
} Precedence;

typedef void (*ParseFn)();

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;


Chunk* compilingChunk;

static Chunk* currentChunk(){
    return compilingChunk;
}

static void errorAt(Token* token, const char* message){
    //panic mode allows us to continue parsing after the error
    parser.panicMode  = true;
    fprintf(stderr, "[line %d] Error", token->line);
    if(token->type ==  TOKEN_EOF){
        fprintf(stderr, " at end");
    } else if (token->type == TOKEN_ERROR){

    } else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }
    fprintf(stderr, ": %s\n", message);
    parser.hadError = true;
}
static void errorAtCurrent(const char* message){
    errorAt(&parser.current, message);
}

static void error(const char* message){
    errorAt(&parser.previous, message);
}

//OP_CONSTANT uses a single byte, can only store and
//load up to 256 bytes in a chunk
static uint8_t makeConstant(Value value){
    int constant = addConstant(currentChunk(), value);
    if(constant > UINT8_MAX){
        error("Too many constants in one chunk");
        return 0;
    }
    return (uint8_t)constant;
}

static void advance(){
    //Store current token
    parser.previous = parser.current;
    //Loop through token until non error token is found or reach the end
    while(true) {
        parser.current = scanToken();
        if(parser.current.type != TOKEN_ERROR) break;

        errorAtCurrent(parser.current.start);
    }
}

static void consume(TokenType type, const char* message){
    if(parser.current.type == type){
        advance();
        return;
    }

    errorAtCurrent(message);
}


static void emitByte(uint8_t byte){
    writeChunk(currentChunk(), byte, parser.previous.line);
}

static void emitBytes(uint8_t byte1, uint8_t byte2){
    emitByte(byte1);
    emitByte(byte2);
}

static void emitReturn(){
    return emitByte(OP_RETURN);
}


static void emitConstant(Value value){
    printf("emit");
    emitBytes(OP_CONSTANT, makeConstant(value));
}

static void endCompiler(){
    emitReturn();
    #ifdef DEBUG_PRINT_CODE
        if(!parser.hadError){
            disassembleChunk(currentChunk(), "code");
        }
    #endif
}


static void expression();
static ParseRule* getRule(TokenType type);
static void parsePrecedence(Precedence precedence);

//Starts at the current token and parses any expression 
//at the given precendence level or higher
static void parsePrecedence(Precedence precedence){
    //Read next token and looks up corresponding parserule
    //First token should ALWAYS be a prefix expression
    advance();
    ParseFn prefixRule = getRule(parser.previous.type)->prefix;
    if(prefixRule == NULL){
        error("Expect expression");
        return;
    }

    prefixRule();

    //If next token is too low precedence, 
    //or isn't an infix operator, we are done

    while(precedence <= getRule(parser.current.type)->precedence) {
        advance();
        ParseFn infixRule = getRule(parser.previous.type)->infix;
        infixRule();
    }
}

static void number(){
    // printf("number");
    double value = strtod(parser.previous.start, NULL);
    emitConstant(NUMBER_VAL(value));
}
static void grouping(){
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression");
}

static void unary(){
    TokenType operatorType = parser.previous.type;

    parsePrecedence(PREC_UNARY);

    switch(operatorType){
        case TOKEN_MINUS: emitByte(OP_NEGATE); break;
        case TOKEN_BANG: emitByte(OP_NOT); break;
        default:
            return;
    }
} 



static void binary(){
    //   printf("binary");
    //Rmr operator
    TokenType operatorType = parser.previous.type;

    //Compile the right operand
    //i.e for 2*3+4, only need 2*3 not 2*(3+4)
    ParseRule* rule = getRule(operatorType);
    parsePrecedence((Precedence)(rule->precedence+1));

    //Emit the operation instruction
    switch(operatorType){
        case TOKEN_PLUS: emitByte(OP_ADD); printf("add"); break;
        case TOKEN_MINUS: emitByte(OP_SUBTRACT); break;
        case TOKEN_STAR: emitByte(OP_MULTIPLY); break;
        case TOKEN_SLASH: emitByte(OP_DIVIDE);break;
        case TOKEN_BANG_EQUAL: emitBytes(OP_EQUAL, OP_NOT); break;
        case TOKEN_EQUAL_EQUAL: emitByte(OP_EQUAL); break;
        case TOKEN_GREATER: emitByte(OP_GREATER); break;
        // >= is the same as NOT <
        case TOKEN_GREATER_EQUAL: emitBytes(OP_LESS, OP_NOT); break;
        case TOKEN_LESS: emitByte(OP_LESS); break;
        case TOKEN_LESS_EQUAL: emitBytes(OP_GREATER,  OP_NOT); break;
        default:
            return;
    }
}

static void literal(){
    switch(parser.previous.type){
        case TOKEN_FALSE: emitByte(OP_FALSE); break;
        case TOKEN_TRUE: emitByte(OP_TRUE);break;
        case TOKEN_NIL: emitByte(OP_NIL); break;
        default:
            return;
    }
}

static void string(){
    emitConstant(OBJ_VAL(copyString(parser.previous.start +1 , parser.previous.length -2)));
}

//Prefix, infix, precedence
ParseRule rules[] = {
    [TOKEN_LEFT_PAREN]    = { grouping, NULL,   PREC_NONE },
  [TOKEN_RIGHT_PAREN]   = { NULL,     NULL,   PREC_NONE },
  [TOKEN_LEFT_BRACE]    = { NULL,     NULL,   PREC_NONE }, 
  [TOKEN_RIGHT_BRACE]   = { NULL,     NULL,   PREC_NONE },
  [TOKEN_COMMA]         = { NULL,     NULL,   PREC_NONE },
  [TOKEN_DOT]           = { NULL,     NULL,   PREC_NONE },
  [TOKEN_MINUS]         = { unary,    binary, PREC_TERM },
  [TOKEN_PLUS]          = { NULL,     binary, PREC_TERM },
  [TOKEN_SEMICOLON]     = { NULL,     NULL,   PREC_NONE },
  [TOKEN_SLASH]         = { NULL,     binary, PREC_FACTOR },
  [TOKEN_STAR]          = { NULL,     binary, PREC_FACTOR },
  [TOKEN_BANG]          = { unary,     NULL,   PREC_NONE },
  [TOKEN_BANG_EQUAL]    = { NULL,     binary,   PREC_EQUALITY },
  [TOKEN_EQUAL]         = { NULL,     NULL,   PREC_NONE },
  [TOKEN_EQUAL_EQUAL]   = { NULL,     binary,   PREC_EQUALITY },
  [TOKEN_GREATER]       = { NULL,     binary,   PREC_COMPARISON },
  [TOKEN_GREATER_EQUAL] = { NULL,     binary,   PREC_COMPARISON },
  [TOKEN_LESS]          = { NULL,     binary,   PREC_COMPARISON },
  [TOKEN_LESS_EQUAL]    = { NULL,     binary,   PREC_COMPARISON },
  [TOKEN_IDENTIFIER]    = { NULL,     NULL,   PREC_NONE },
  [TOKEN_STRING]        = { string,     NULL,   PREC_NONE },
  [TOKEN_NUMBER]        = { number,   NULL,   PREC_NONE },
  [TOKEN_AND]           = { NULL,     NULL,   PREC_NONE },
  [TOKEN_CLASS]         = { NULL,     NULL,   PREC_NONE },
  [TOKEN_ELSE]          = { NULL,     NULL,   PREC_NONE },
  [TOKEN_FALSE]         = { literal,     NULL,   PREC_NONE },
  [TOKEN_FOR]           = { NULL,     NULL,   PREC_NONE },
  [TOKEN_FUN]           = { NULL,     NULL,   PREC_NONE },
  [TOKEN_IF]            = { NULL,     NULL,   PREC_NONE },
  [TOKEN_NIL]           = { literal,     NULL,   PREC_NONE },
  [TOKEN_OR]            = { NULL,     NULL,   PREC_NONE },
  [TOKEN_PRINT]         = { NULL,     NULL,   PREC_NONE },
  [TOKEN_RETURN]        = { NULL,     NULL,   PREC_NONE },
  [TOKEN_SUPER]         = { NULL,     NULL,   PREC_NONE },
  [TOKEN_THIS]          = { NULL,     NULL,   PREC_NONE },
  [TOKEN_TRUE]          = { literal,     NULL,   PREC_NONE },
  [TOKEN_VAR]           = { NULL,     NULL,   PREC_NONE },
  [TOKEN_WHILE]         = { NULL,     NULL,   PREC_NONE },
  [TOKEN_ERROR]         = { NULL,     NULL,   PREC_NONE },
  [TOKEN_EOF]           = { NULL,     NULL,   PREC_NONE },
};

static ParseRule* getRule(TokenType type){
    return &rules[type];
}


static void expression(){
    parsePrecedence(PREC_ASSIGNMENT);
}



bool compile(const char* source, Chunk* chunk){
    initScanner(source);
    compilingChunk = chunk;
    parser.hadError = false;
    parser.panicMode = false;
    advance();
    expression();
    consume(TOKEN_EOF, "Expect end of Expression");
    endCompiler();
    return !parser.hadError;
}