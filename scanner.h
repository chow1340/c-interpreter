#ifndef cInterp_scanner_h
#define cInterp_scanner_h

typedef struct {
    const char* start; //marks the beginning of the current lexeme
    const char* current; //current character being looked at
    int line; // track which line the current lexeme is on
} Scanner;

typedef enum {
    //Single-char
    TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN, TOKEN_LEFT_BRACE,TOKEN_RIGHT_BRACE,
    TOKEN_COMMA, TOKEN_DOT, TOKEN_MINUS, TOKEN_PLUS, 
    TOKEN_SEMICOLON, TOKEN_SLASH, TOKEN_STAR,

    //One or two char
    TOKEN_BANG, TOKEN_BANG_EQUAL, TOKEN_EQUAL, TOKEN_EQUAL_EQUAL, 
    TOKEN_GREATER, TOKEN_GREATER_EQUAL, TOKEN_LESS, TOKEN_LESS_EQUAL,
    TOKEN_PLUS_PLUS,

    //Literals
    TOKEN_IDENTIFIER, TOKEN_STRING, TOKEN_NUMBER,

    //Keywords
    TOKEN_AND, TOKEN_CLASS, TOKEN_ELSE, TOKEN_FALSE,
    TOKEN_FOR, TOKEN_FUN, TOKEN_IF, TOKEN_NIL, TOKEN_OR,
    TOKEN_PRINT, TOKEN_RETURN, TOKEN_SUPER, TOKEN_THIS,
    TOKEN_TRUE, TOKEN_VAR, TOKEN_WHILE,

    TOKEN_ERROR,
    TOKEN_EOF
} TokenType;

typedef struct {
    TokenType type;
    const char* start;
    int length;
    int line;
}Token;


void initScanner(const char* source);
Token scanToken();

#endif