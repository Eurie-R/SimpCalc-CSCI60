/*
 * scanner.c - SimpCalc scanner (lexical analyzer).
 *
 * gettoken() is a direct translation of our DFA (DFA.pdf). There is one
 * `case` per circle of the DFA, named after its letter (S_A = state A,
 * S_B = state B, ...), and each `if` inside a case is one arrow.
 *
 *   Token without pushback": the last character read is
 *     part of the token, so we return right away.
 *
 *  "Token with pushback": we only know the token ended
 *     because we read a character that is NOT part of it, so we push that
 *     character back for the next call.
 *
 *  "Error" (red circles): we return an Error token. The character that
 *     caused the error is consumed.

 */
#include <ctype.h>
#include <string.h>
#include "scanner.h"


#define MATCH_REFERENCE_LINE_NUMBERS 1

// DFA states, named after the circles in DFA.pdf.
typedef enum {
    S_A,        // A: start state
    S_IDENT,    // Identifier: letters, digits, underscores    
    S_B,        // B: whole number         
    S_C,        // C: seen "digits." - a digit must follow   
    S_D,        // D: float 
    S_E,        // E: seen 'e' or 'E' 
    S_F,        // F: seen sign after 'e' 
    S_G,        // G: exponent digits
    S_H,        // H: inside a string 
    S_I,        // I: seen '/'
    S_COMMENT,  // Comment: skipping until end of line  
    S_J,        // J: seen '*'    
    S_K,        // K: seen '>'  
    S_L,        // L: seen '<'  
    S_M,        // M: seen ':'  
    S_N         // N: seen '!'   
} State;

static FILE *src;               // file being scanned  
static int   line;              // current line counter 
static int   pushed_char;       // one-character pushback buffer
static int   has_pushback;      // 1 if pushed_char holds a character 
static int   pending_dot_error; // 1 if the last Number ended at a '.'

static char  lexeme[MAX_LEXEME];
static int   lex_len;

// Keyword table: exact, case-sensitive matches.
static const struct { const char *word; TokenType type; } keywords[] = {
    { "PRINT", T_PRINT }, { "IF",   T_IF   }, { "ELSE", T_ELSE },
    { "ENDIF", T_ENDIF }, { "SQRT", T_SQRT }, { "AND",  T_AND  },
    { "OR",    T_OR    }, { "NOT",  T_NOT  }
};

// Returns the printable name of a token type (used in all output).
const char *token_name(TokenType t) {
    switch (t) {
    case T_IDENTIFIER:  return "Identifier";
    case T_NUMBER:      return "Number";
    case T_STRING:      return "String";
    case T_ASSIGN:      return "Assign";
    case T_SEMICOLON:   return "Semicolon";
    case T_COLON:       return "Colon";
    case T_COMMA:       return "Comma";
    case T_LEFTPAREN:   return "LeftParen";
    case T_RIGHTPAREN:  return "RightParen";
    case T_PLUS:        return "Plus";
    case T_MINUS:       return "Minus";
    case T_MULTIPLY:    return "Multiply";
    case T_DIVIDE:      return "Divide";
    case T_RAISE:       return "Raise";
    case T_LESSTHAN:    return "LessThan";
    case T_EQUAL:       return "Equal";
    case T_GREATERTHAN: return "GreaterThan";
    case T_LTEQUAL:     return "LTEqual";
    case T_GTEQUAL:     return "GTEqual";
    case T_NOTEQUAL:    return "NotEqual";
    case T_PRINT:       return "Print";
    case T_IF:          return "If";
    case T_ELSE:        return "Else";
    case T_ENDIF:       return "Endif";
    case T_SQRT:        return "Sqrt";
    case T_AND:         return "And";
    case T_OR:          return "Or";
    case T_NOT:         return "Not";
    case T_EOF:         return "EndofFile";
    case T_ERROR:       return "Error";
    }
    return "Unknown";
}

// starts scanning a new file and resets all scanner state.
void scanner_init(FILE *in) {
    src = in;
    line = 1;
    has_pushback = 0;
    pending_dot_error = 0;
    lex_len = 0;
    lexeme[0] = '\0';
}

// reads the next character, taking the pushed-back one first.
// carriage returns are dropped so CRLF files behave like LF files.
static int next_char(void) {
    int c;
    if (has_pushback) {
        has_pushback = 0;
        return pushed_char;
    }
    do {
        c = fgetc(src);
    } while (c == '\r');
    return c;
}

// "Un-reads" one character so the next next_char() returns it (EOF too). 
static void push_back(int c) {
    pushed_char = c;
    has_pushback = 1;
}

// empties the lexeme buffer.
static void clear_lexeme(void) {
    lex_len = 0;
    lexeme[0] = '\0';
}

// appends c to the lexeme; characters beyond MAX_LEXEME-1 are dropped.
static void add(int c) {
    if (lex_len < MAX_LEXEME - 1) {
        lexeme[lex_len++] = (char)c;
        lexeme[lex_len] = '\0';
    }
}

// builds a token of the given type from the current lexeme and line.
static Token make_token(TokenType type) {
    Token t;
    t.type = type;
    strcpy(t.lexeme, lexeme);
    t.line = line;
    t.error = ERR_NONE;
    return t;
}

// builds the token for an accepted identifier: a keyword type if the
// lexeme is one of the keywords, otherwise T_IDENTIFIER.
static Token identifier_or_keyword(void) {
    size_t i;
    for (i = 0; i < sizeof keywords / sizeof keywords[0]; i++) {
        if (strcmp(lexeme, keywords[i].word) == 0)
            return make_token(keywords[i].type);
    }
    return make_token(T_IDENTIFIER);
}

// builds an error token for the offending character c, which is consumed.
// the token carries the line BEFORE any newline adjustment below.
// see MATCH_REFERENCE_LINE_NUMBERS for the newline handling.
static Token lex_error(LexErrorKind kind, int c, int in_string) {
    Token t = make_token(T_ERROR);
    t.error = kind;
    if (c == EOF) {
        push_back(EOF);                 /* next call returns EndofFile */
    } else if (c == '\n') {
        if (in_string) {
            /* Unterminated string: the reference counts this newline twice. */
#if MATCH_REFERENCE_LINE_NUMBERS
            line++;
#endif
            push_back('\n');            /* state A counts it (again) */
        } else {
            /* Bad number / bad '!' swallowed the newline:
               the reference does not count it. */
#if !MATCH_REFERENCE_LINE_NUMBERS
            line++;
#endif
        }
    }
    return t;
}

// handles a Number that ends at character c (states D and G):
// push c back, and remember if it was a '.' (e.g. "1e1." or
// "111.222e333.444") so that '.' is reported as an invalid number
// format rather than an illegal character.
static Token end_number(int c) {
    push_back(c);
    if (c == '.')
        pending_dot_error = 1;
    return make_token(T_NUMBER);
}

// state A: the start state. Returns 1 and sets *tok if a token (or error)
// is finished here; otherwise sets *state to the next state and returns 0.
static int start_state(int c, int dot_after_number, State *state, Token *tok) {
    if (c == ' ' || c == '\t')
        return 0;                                   /* stay in A */
    if (c == '\n') {
        line++;
        return 0;                                   /* stay in A */
    }
    if (c == EOF) {
        add(' ');                                   /* EOF lexeme is one space */
        *tok = make_token(T_EOF);
        return 1;
    }
    if (isalpha(c) || c == '_') { add(c); *state = S_IDENT; return 0; }
    if (isdigit(c))             { add(c); *state = S_B;     return 0; }

    switch (c) {
    case '"': add(c); *state = S_H; return 0;
    case '/': add(c); *state = S_I; return 0;
    case '*': add(c); *state = S_J; return 0;
    case '>': add(c); *state = S_K; return 0;
    case '<': add(c); *state = S_L; return 0;
    case ':': add(c); *state = S_M; return 0;
    case '!': add(c); *state = S_N; return 0;
    case ';': add(c); *tok = make_token(T_SEMICOLON);  return 1;
    case ',': add(c); *tok = make_token(T_COMMA);      return 1;
    case '(': add(c); *tok = make_token(T_LEFTPAREN);  return 1;
    case ')': add(c); *tok = make_token(T_RIGHTPAREN); return 1;
    case '+': add(c); *tok = make_token(T_PLUS);       return 1;
    case '-': add(c); *tok = make_token(T_MINUS);      return 1;
    case '=': add(c); *tok = make_token(T_EQUAL);      return 1;
    }

    if (c == '.' && dot_after_number)
        *tok = lex_error(ERR_NUMBER, c, 0);
    else
        *tok = lex_error(ERR_ILLEGAL, c, 0);
    return 1;
}

// returns the next token from the input by running the DFA.
Token gettoken(void) {
    State state = S_A;
    int   dot_after_number = pending_dot_error;
    Token tok;

    pending_dot_error = 0;
    clear_lexeme();

    for (;;) {
        int c = next_char();

        switch (state) {
        case S_A:
            if (start_state(c, dot_after_number, &state, &tok))
                return tok;
            break;

        case S_IDENT:                               /* Identifier */
            if (isalnum(c) || c == '_') { add(c); break; }
            push_back(c);
            return identifier_or_keyword();

        case S_B:                                   /* B (whole) */
            if (isdigit(c))             { add(c); break; }
            if (c == '.')               { add(c); state = S_C; break; }
            if (c == 'e' || c == 'E')   { add(c); state = S_E; break; }
            push_back(c);
            return make_token(T_NUMBER);

        case S_C:                                   /* C: needs a digit */
            if (isdigit(c))             { add(c); state = S_D; break; }
            return lex_error(ERR_NUMBER, c, 0);

        case S_D:                                   /* D (float) */
            if (isdigit(c))             { add(c); break; }
            if (c == 'e' || c == 'E')   { add(c); state = S_E; break; }
            return end_number(c);

        case S_E:                                   /* E: seen e/E */
            if (isdigit(c))             { add(c); state = S_G; break; }
            if (c == '+' || c == '-')   { add(c); state = S_F; break; }
            return lex_error(ERR_NUMBER, c, 0);

        case S_F:                                   /* F: seen sign */
            if (isdigit(c))             { add(c); state = S_G; break; }
            return lex_error(ERR_NUMBER, c, 0);

        case S_G:                                   /* G (exp) */
            if (isdigit(c))             { add(c); break; }
            return end_number(c);

        case S_H:                                   /* H: inside a string */
            if (c == '"') { add(c); return make_token(T_STRING); }
            if (c == '\n' || c == EOF)
                return lex_error(ERR_UNTERMINATED, c, 1);
            add(c);
            break;

        case S_I:                                   /* I: seen '/' */
            if (c == '/') { clear_lexeme(); state = S_COMMENT; break; }
            push_back(c);
            return make_token(T_DIVIDE);

        case S_COMMENT:                             /* Comment */
            if (c == '\n') { push_back(c); clear_lexeme(); state = S_A; break; }
            if (c == EOF)  { add(' '); return make_token(T_EOF); }
            break;

        case S_J:                                   /* J: seen '*' */
            if (c == '*') { add(c); return make_token(T_RAISE); }
            push_back(c);
            return make_token(T_MULTIPLY);

        case S_K:                                   /* K: seen '>' */
            if (c == '=') { add(c); return make_token(T_GTEQUAL); }
            push_back(c);
            return make_token(T_GREATERTHAN);

        case S_L:                                   /* L: seen '<' */
            if (c == '=') { add(c); return make_token(T_LTEQUAL); }
            push_back(c);
            return make_token(T_LESSTHAN);

        case S_M:                                   /* M: seen ':' */
            if (c == '=') { add(c); return make_token(T_ASSIGN); }
            push_back(c);
            return make_token(T_COLON);

        case S_N:                                   /* N: seen '!' */
            if (c == '=') { add(c); return make_token(T_NOTEQUAL); }
            return lex_error(ERR_BANG, c, 0);
        }
    }
}
