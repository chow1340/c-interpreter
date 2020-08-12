#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

typedef struct {
    Token name;
    int depth;
} Local;

typedef enum {
    TYPE_FUNCTION,
    TYPE_SCRIPT
} FunctionType;

typedef struct{
    struct Compiler* enclosing;
    ObjFunction* function;
    FunctionType type;
    Local locals[UINT8_COUNT];
    int localCount;
    int scopeDepth; // 0 = global, 1 = first top level, 2 = second, etc...
}  Compiler;

Compiler* current = NULL;
Chunk* compilingChunk;

static Chunk* currentChunk(){
    return &current->function->chunk;
}

static void initCompiler(Compiler* compiler, FunctionType type) {
    compiler->enclosing = current;
    compiler->function = NULL; //garbage collection
    compiler->type = type;
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
    compiler->function = newFunction();
    current = compiler;

    if(type != TYPE_SCRIPT) {
        current->function->name = copyString(parser.previous.start, parser.previous.length);
    }
    //claims stack slot zero for VM's own internal use
    Local* local = &current->locals[current->localCount++];
    local->depth = 0;
    local->name.start = "";
    local->name.length = 0;
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

static void emitLoop(int loopStart){
    emitByte(OP_LOOP); //emit new loop insturction 

    
    int offset = currentChunk()->count-loopStart + 2;
    if(offset > UINT16_MAX) error("Loop body too large");

    emitByte((offset >> 8) & 0xff);
    emitByte(offset & 0xff);
}

static void emitReturn(){
    emitByte(OP_NIL);
    emitByte(OP_RETURN);
}


static void emitConstant(Value value){
    emitBytes(OP_CONSTANT, makeConstant(value));
}

//Goes back into the bytecode and replaces operand at the given location
//with the calculated offset.
static void patchJump(int offset){
    // -2 to adjust for bytecode for offset jump itself
    int jump = currentChunk()->count-offset-2;
    if(jump > UINT16_MAX) {
        error("Too much code to jump over");
    }
    //replace the two bytes with then statement
    currentChunk()->code[offset] = (jump >> 8) & 0xff;
    currentChunk()->code[offset+1] = jump & 0xff;
}

static int emitJump(uint8_t instruction) {
    emitByte(instruction); //placeholder operand
    //Two bytes for jump offset
    emitByte(0xff); 
    emitByte(0xff);
    return currentChunk()->count - 2;
}

static ObjFunction* endCompiler(){
    emitReturn();
    ObjFunction* function = current->function;
    #ifdef DEBUG_PRINT_CODE
        if(!parser.hadError){
            disassembleChunk(currentChunk(), 
            function->name != NULL ? function->name->chars : "<script>" );
        }
    #endif

    current = current->enclosing;
    return function;
}

static void beginScope(){
    current->scopeDepth++;
}

static void endScope(){
    current->scopeDepth--;

    //Pop out the variable when out of block
    while(current->localCount > 0 && 
    current->locals[current->localCount - 1].depth > 
    current->scopeDepth) {
        emitByte(OP_POP);
        current->localCount--;
    }
}


static void expression();
static ParseRule* getRule(TokenType type);
static void parsePrecedence(Precedence precedence);
static void statement();
static void declaration();

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

static void addLocal(Token name){
    if(current->localCount ==  UINT8_COUNT) {
        error("Too many local variables in function");
        return;
    }
    Local* local  = &current->locals[current->localCount++];
    local->name = name;
    local->depth = -1;
}

static bool identifiersEqual(Token* a, Token* b) {
    if(a->length != b->length) return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

static int resolveLocal(Compiler* compiler, Token* name) {
    for(int i = compiler->localCount-1; i >= 0; i--){
        Local* local = &compiler->locals[i];
        if(identifiersEqual(&local->name, name)){
            if(local->depth == -1) {
                error("Cannot read local variable in its own initializer");
            }
            return i;
        }
    }
    return -1;
}

//Records existence of variable, only for locals
static void declareVariable(){
    //Global variables are implicitly declared
    if(current->scopeDepth == 0) return;

    Token* name = &parser.previous;

    //Loop through array of local to see if a var declared alrdy in the local scope
    for(int i = current->localCount - 1; i >= 0; i--) {
        Local* local = &current->locals[i];
        if(local->depth != -1 && local->depth < current->scopeDepth) {
            break;
        }
        if(identifiersEqual(name, &local->name)) {
            error("Variable withthis name already declared in this scope");
        }
    }
    addLocal(*name);
}

//Consumes token identifier for the variable name,
//and adds its lexeme to the chunk's constant table as a string,
//Then returns the index of the constant
static uint8_t parseVariable(const char* errorMessage){
    consume(TOKEN_IDENTIFIER, errorMessage);

    declareVariable();

    //exit function if in a local scope
    if(current->scopeDepth > 0) return 0; 

    return identifierConstant(&parser.previous);
}

static void markInitialized(){
    if(current->scopeDepth == 0) return;
    current->locals[current->localCount-1].depth = current->scopeDepth;
}

//Emits the bytecode
static void defineVariable(uint8_t global){
    //Do not store if in local scope
    if(current->scopeDepth > 0) {
        markInitialized();
        return;
    }
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
        case TOKEN_PLUS: emitByte(OP_ADD); break;
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
    uint8_t getOp, setOp;
    int arg = resolveLocal(current, &name);
    if(arg != -1) {
        getOp = OP_GET_LOCAL;
        setOp = OP_SET_LOCAL;
    } else {
        arg = identifierConstant(&name);
        getOp = OP_GET_GLOBAL;
        setOp = OP_SET_GLOBAL;
    }
    
    if(canAssign && match(TOKEN_EQUAL)) {
        //If something like a.b().c = d,
        // 'c' should be a setter, not a getter so 
        // need to compile and then set as a variable
        expression();
        emitBytes(setOp, (uint8_t)arg);
    } else {
        emitBytes(getOp, (uint8_t)arg);
    }
}

static void variable(bool canAssign){
    namedVariable(parser.previous, canAssign);
}

static void and_(bool canAssign){
    //If false, jump
    int endJump = emitJump(OP_JUMP_IF_FALSE);

    //Else discard left expression and evaluate right side
    emitByte(OP_POP);
    parsePrecedence(PREC_AND);

    patchJump(endJump);
}

static void or_(bool canAssign) {
    //if left truthy, skip right hand operand
    int elseJump = emitJump(OP_JUMP_IF_FALSE);
    //if left is false, continue
    int endJump = emitJump(OP_JUMP);

    patchJump(elseJump);
    emitByte(OP_POP);
    parsePrecedence(PREC_OR);
    patchJump(endJump);
}

static uint8_t argumentList(){
    uint8_t argCount = 0;
    if(!check(TOKEN_RIGHT_PAREN)){
        do{
            expression();
            if(argCount == 255) {
                error("Cannot have more than 255 arguements");
            }
            argCount++;
        }while(match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments");
    return argCount;
}

static void call(bool canAssign){
    uint8_t argCount = argumentList();
    emitBytes(OP_CALL, argCount);
}

//Prefix, infix, precedence
ParseRule rules[] = {
    [TOKEN_LEFT_PAREN]    = { grouping, call,   PREC_CALL },
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
  [TOKEN_AND]           = { NULL,     and_,   PREC_AND },
  [TOKEN_CLASS]         = { NULL,     NULL,   PREC_NONE },
  [TOKEN_ELSE]          = { NULL,     NULL,   PREC_NONE },
  [TOKEN_FALSE]         = { literal,     NULL,   PREC_NONE },
  [TOKEN_FOR]           = { NULL,     NULL,   PREC_NONE },
  [TOKEN_FUN]           = { NULL,     NULL,   PREC_NONE },
  [TOKEN_IF]            = { NULL,     NULL,   PREC_NONE },
  [TOKEN_NIL]           = { literal,     NULL,   PREC_NONE },
  [TOKEN_OR]            = { NULL,     or_,   PREC_OR },
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

static void block(){
    while(!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
        declaration();
    }

    consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

//Compile the function itself
static void function(FunctionType type){
    //Create a seperate compiler for each function
    Compiler compiler;
    initCompiler(&compiler, type);
    beginScope();

    //Compile parameters
    consume(TOKEN_LEFT_PAREN, "Expect '(' after function name");
    if(!check(TOKEN_RIGHT_PAREN)){
        do{
            current->function->arity++;
            if(current->function->arity > 255){
                errorAtCurrent("Cannot have more than 255 parameters");
            }

            uint8_t paramConstant = parseVariable("Expect a variable name");
            defineVariable(paramConstant);
        } while(match(TOKEN_COMMA));    
    } 
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters");

    //Compile body
    consume(TOKEN_LEFT_BRACE, "Expect '{' before function body");
    block();

    //Create the function object;
    ObjFunction* function = endCompiler();
    emitBytes(OP_CONSTANT, makeConstant(OBJ_VAL(function)));
}
static void funDeclaration(){
    uint8_t global = parseVariable("Expect a function name");
    markInitialized();
    function(TYPE_FUNCTION);
    defineVariable(global);
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

static void ifStatement(){
    //Compile condition expression
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    //Offset to jump to if false
    //Set an placeholder offset with thenJump, compile then statement,
    //then come back and replace placeholder offset  with real one
    int thenJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP); // pop out the condition 
    statement();
    int elseJump = emitJump(OP_JUMP);
    patchJump(thenJump);
    emitByte(OP_POP); 
    if(match(TOKEN_ELSE)) statement();
    patchJump(elseJump);

}

static void printStatement(){
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after value");
    emitByte(OP_PRINT);
}

static void whileStatement(){
    //Mark the start chunk
    int loopStart = currentChunk()->count;
    consume(TOKEN_LEFT_PAREN, "Expect '(' after while");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition");

    //Exit if condition false
    int exitJump = emitJump(OP_JUMP_IF_FALSE);

    emitByte(OP_POP);
    statement();

    emitLoop(loopStart);
    patchJump(exitJump);
    emitByte(OP_POP);
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
    } else if (match(TOKEN_FUN)) {
        funDeclaration();
    }
    else {
        statement();
    }
    if(parser.panicMode) synchronize();
}

static void returnStatement(){
    if(current->type == TYPE_SCRIPT) {
        error("Cannot return from top level code");
    }
    if(match(TOKEN_SEMICOLON)){
        emitReturn();
    } else {
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after return value");
        emitByte(OP_RETURN);
    }
}

static void statement(){
    if(match(TOKEN_PRINT)){
        printStatement();
    } else if (match(TOKEN_LEFT_BRACE)){
        beginScope();
        block();
        endScope();
    } else if (match(TOKEN_IF)){
        ifStatement();
    } else if (match(TOKEN_WHILE)) {
        whileStatement();
    } else if (match(TOKEN_RETURN)) {
        returnStatement();
    } else {
        expressionStatement();
    }
}

ObjFunction* compile(const char* source){
    initScanner(source);
    Compiler compiler;
    initCompiler(&compiler, TYPE_SCRIPT);
    // compilingChunk = chunk;
    parser.hadError = false;
    parser.panicMode = false;
    advance();
    while(!match(TOKEN_EOF)) {
        declaration();
    }
    ObjFunction* function = endCompiler();
    return parser.hadError ? NULL : function;
}