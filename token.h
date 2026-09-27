/*
 * token.h - Token definitions shared by the SimpCalc scanner and parser.
 *
 * A Token is what gettoken() hands out: its type, the characters it was
 * made from (the lexeme), the line it was found on, and - for error
 * tokens only - which kind of lexical error it is.
 */
#ifndef TOKEN_H
#define TOKEN_H

// every kind of token the scanner can produce.
typedef enum {
    T_IDENTIFIER, T_NUMBER, T_STRING,
    T_ASSIGN, T_SEMICOLON, T_COLON, T_COMMA, T_LEFTPAREN, T_RIGHTPAREN,
    T_PLUS, T_MINUS, T_MULTIPLY, T_DIVIDE, T_RAISE,
    T_LESSTHAN, T_EQUAL, T_GREATERTHAN, T_LTEQUAL, T_GTEQUAL, T_NOTEQUAL,
    T_PRINT, T_IF, T_ELSE, T_ENDIF, T_SQRT, T_AND, T_OR, T_NOT,
    T_EOF, T_ERROR
} TokenType;

// kinds of lexical error. Only meaningful when type == T_ERROR.
typedef enum {
    ERR_NONE,
    ERR_ILLEGAL,          // illegal character, e.g. '.', '#', '@'
    ERR_NUMBER,           // invalid number format, e.g. "76.", "3e+x"
    ERR_UNTERMINATED,     // string reaches end of line / file
    ERR_BANG              // '!' not followed by '='
} LexErrorKind;

#define MAX_LEXEME 1024

typedef struct {
    TokenType    type;
    char         lexeme[MAX_LEXEME];
    int          line;    /* line number to report for this token */
    LexErrorKind error;
} Token;

// Printable name of a token type, e.g. "Identifier", "GTEqual".
// Used in the scan output and in parser "X Expected" messages.
// Defined in scanner.c.
const char *token_name(TokenType t);

#endif /* TOKEN_H */
