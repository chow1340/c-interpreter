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

typedef void (*ParseFn)(bool canAssign);

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

static bool check(TokenType type) {
    return parser.current.type == type;
}

static bool match(TokenType type){
    if(!check(type)) return false;
    advance();
    return true;
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
static void statement();
static void  declaration();

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

    bool canAssign = precedence <=  PREC_ASSIGNMENT;
    prefixRule(canAssign);

    //If next token is too low precedence, 
    //or isn't an infix operator, we are done

    while(precedence <= getRule(parser.current.type)->precedence) {
        advance();
        ParseFn infixRule = getRule(parser.previous.type)->infix;
        infixRule(canAssign);
    }

    if(canAssign && match(TOKEN_EQUAL)){
        error("Invalid assignment target.");
    }
}

static uint8_t identifierConstant(Token* name){
    return makeConstant(OBJ_VAL(copyString(name->start, name->length)));
}

static uint8_t parseVariable(const char* errorMessage){
    consume(TOKEN_IDENTIFIER, errorMessage);
    return identifierConstant(&parser.previous);
}

static void defineVariable(uint8_t global){
    emitBytes(OP_DEFINE_GLOBAL, global);
}

static void number(bool canAssign){
    // printf("number");
    double value = strtod(parser.previous.start, NULL);
    emitConstant(NUMBER_VAL(value));
}
static void grouping(bool canAssign){
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression");
}

static void unary(bool canAssign){
    TokenType operatorType = parser.previous.type;

    parsePrecedence(PREC_UNARY);

    switch(operatorType){
        case TOKEN_MINUS: emitByte(OP_NEGATE); break;
        case TOKEN_BANG: emitByte(OP_NOT); break;
        default:
            return;
    }
} 



static void binary(bool canAssign){
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

static void literal(bool canAssign){
    switch(parser.previous.type){
        case TOKEN_FALSE: emitByte(OP_FALSE); break;
        case TOKEN_TRUE: emitByte(OP_TRUE);break;
        case TOKEN_NIL: emitByte(OP_NIL); break;
        default:
            return;
    }
}

static void string(bool canAssign){
    emitConstant(OBJ_VAL(copyString(parser.previous.start +1 , parser.previous.length -2)));
}

static void namedVariable(Token name, bool canAssign){
    uint8_t arg = identifierConstant(&name);
    
    if(canAssign && match(TOKEN_EQUAL)) {
        //If something like a.b().c = d,
        // 'c' should be a setter, not a getter so 
        // need to compile and then set as a variable
        expression();
        emitBytes(OP_SET_GLOBAL, arg);
    } else {
        emitBytes(OP_GET_GLOBAL, arg);
    }
}

static void variable(bool canAssign){
    namedVariable(parser.previous, canAssign);
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
  [TOKEN_IDENTIFIER]    = { variable,     NULL,   PREC_NONE },
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

static void varDeclarations(){
    uint8_t global = parseVariable("Expect variable name");

    if(match(TOKEN_EQUAL)) {
        expression();
    } else {
        emitByte(OP_NIL);
    }

    consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration");

    defineVariable(global);
}

//Evaluates the expression and then discards the result
//i.e brunch = "bagel"; eat(brunch);
static void expressionStatement(){
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after expression");
    emitByte(OP_POP);
}

static void printStatement(){
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after value");
    emitByte(OP_PRINT);
}

//Skips tokens until it looks like a token that ends a statement,
//or something that begins a statement
static void synchronize(){
    parser.panicMode = false;

    while(parser.current.type != TOKEN_EOF) {
        if(parser.previous.type == TOKEN_SEMICOLON) return;

        switch(parser.current.type) {
            case TOKEN_CLASS:
            case TOKEN_FUN:
            case TOKEN_VAR:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_PRINT:
            case TOKEN_RETURN:
                return;
            default:
                //Do nothing
                ;
        }

        advance();
    }
}

static void declaration(){
    if(match(TOKEN_VAR)) {
        varDeclarations();
    } else {
        statement();
    }
    if(parser.panicMode) synchronize();
}


static void statement(){
    if(match(TOKEN_PRINT)){
        printStatement();
    } else{
        expressionStatement();
    }
}

bool compile(const char* source, Chunk* chunk){
    initScanner(source);
    compilingChunk = chunk;
    parser.hadError = false;
    parser.panicMode = false;
    advance();
    while(!match(TOKEN_EOF)) {
        declaration();
    }
    endCompiler();
    return !parser.hadError;
}