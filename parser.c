/*
 * recursive-descent parser.
 */
#include <setjmp.h>
#include "parser.h"
#include "scanner.h"

static Token   cur;          // current (lookahead) token
static FILE   *out;          // where parser messages are written
static jmp_buf on_error;     // where to jump when a syntax error hits

// one function per nonterminal.
static void Prg(const char *name);
static void Blk(void);
static void Stm(void);
static void Argfollow(void);
static void Arg(void);
static void Iffollow(void);
static void Exp(void);
static void Trmfollow(void);
static void Trm(void);
static void Facfollow(void);
static void Fac(void);
static void Litfollow(void);
static void Lit(void);
static void Val(void);
static void Cnd(void);
static void Rel(void);

// moves to the next token.
static void advance(void) {
    cur = gettoken();
}

// reports a syntax error on the line of the current token and stops parsing.
static void parse_error(const char *msg) {
    fprintf(out, "Parse Error on line %d: %s.\n", cur.line, msg);
    longjmp(on_error, 1);
}

// the current token must be `expected`; if so, move past it,
//   otherwise report "<Symbol> Expected".
static void match(TokenType expected) {
    char msg[64];
    if (cur.type == expected) {
        advance();
        return;
    }
    snprintf(msg, sizeof msg, "%s Expected", token_name(expected));
    parse_error(msg);
}

int parse_file(FILE *in, FILE *o, const char *display_name) {
    out = o;
    scanner_init(in);
    advance();
    if (setjmp(on_error) != 0)
        return 0;                   // we land here after parse_error()
    Prg(display_name);
    return 1;
}

// Prg -> Blk EndOfFile
static void Prg(const char *name) {
    Blk();
    match(T_EOF);
    fprintf(out, "%s is a valid SimpCalc program\n", name);
}

// Blk -> Stm Blk   {Identifier, PRINT, IF}
//       -> e
static void Blk(void) {
    if (cur.type == T_IDENTIFIER || cur.type == T_PRINT || cur.type == T_IF) {
        Stm();
        Blk();
    }
}

// Stm -> Identifier := Exp ;          {Identifier}
//       -> PRINT ( Arg Argfollow ) ;    {PRINT}
//       -> IF Cnd : Blk Iffollow        {IF}
static void Stm(void) {
    if (cur.type == T_IDENTIFIER) {
        match(T_IDENTIFIER);
        match(T_ASSIGN);
        Exp();
        match(T_SEMICOLON);
        fprintf(out, "Assignment Statement Recognized\n");
    } else if (cur.type == T_PRINT) {
        match(T_PRINT);
        match(T_LEFTPAREN);
        Arg();
        Argfollow();
        match(T_RIGHTPAREN);
        match(T_SEMICOLON);
        fprintf(out, "Print Statement Recognized\n");
    } else if (cur.type == T_IF) {
        fprintf(out, "If Statement Begins\n");
        match(T_IF);
        Cnd();
        match(T_COLON);
        Blk();
        Iffollow();
        fprintf(out, "If Statement Ends\n");
    } else {
        parse_error("Invalid Statement");
    }
}

// Argfollow -> , Arg Argfollow   {Comma}
//             -> e
static void Argfollow(void) {
    if (cur.type == T_COMMA) {
        match(T_COMMA);
        Arg();
        Argfollow();
    }
}

// Arg -> String   {String}
//       -> Exp
static void Arg(void) {
    if (cur.type == T_STRING)
        match(T_STRING);
    else
        Exp();
}

// Iffollow -> ENDIF ;             {ENDIF}
//            -> ELSE Blk ENDIF ;    {ELSE}
//   Any failure here is reported as "Incomplete if Statement".
static void Iffollow(void) {
    if (cur.type == T_ENDIF) {
        match(T_ENDIF);
        match(T_SEMICOLON);
    } else if (cur.type == T_ELSE) {
        match(T_ELSE);
        Blk();
        if (cur.type != T_ENDIF)
            parse_error("Incomplete if Statement");
        advance();
        if (cur.type != T_SEMICOLON)
            parse_error("Incomplete if Statement");
        advance();
    } else {
        parse_error("Incomplete if Statement");
    }
}

// Exp -> Trm Trmfollow
static void Exp(void) {
    Trm();
    Trmfollow();
}

// Trmfollow -> + Trm Trmfollow   {Plus}
//             -> - Trm Trmfollow   {Minus}
//             -> e
static void Trmfollow(void) {
    if (cur.type == T_PLUS) {
        match(T_PLUS);
        Trm();
        Trmfollow();
    } else if (cur.type == T_MINUS) {
        match(T_MINUS);
        Trm();
        Trmfollow();
    }
}

// Trm -> Fac Facfollow
static void Trm(void) {
    Fac();
    Facfollow();
}

// Facfollow -> * Fac Facfollow   {Multiply}
//             -> / Fac Facfollow   {Divide}
//             -> e
static void Facfollow(void) {
    if (cur.type == T_MULTIPLY) {
        match(T_MULTIPLY);
        Fac();
        Facfollow();
    } else if (cur.type == T_DIVIDE) {
        match(T_DIVIDE);
        Fac();
        Facfollow();
    }
}

// Fac -> Lit Litfollow
static void Fac(void) {
    Lit();
    Litfollow();
}

// Litfollow -> ** Lit Litfollow   {Raise}
//             -> e
static void Litfollow(void) {
    if (cur.type == T_RAISE) {
        match(T_RAISE);
        Lit();
        Litfollow();
    }
}

// Lit -> - Val   {Minus}
//       -> Val
static void Lit(void) {
    if (cur.type == T_MINUS) {
        match(T_MINUS);
        Val();
    } else {
        Val();
    }
}

// Val -> Identifier     {Identifier}
//       -> Number         {Number}
//       -> SQRT ( Exp )   {SQRT}
//       -> ( Exp )
static void Val(void) {
    if (cur.type == T_IDENTIFIER) {
        match(T_IDENTIFIER);
    } else if (cur.type == T_NUMBER) {
        match(T_NUMBER);
    } else if (cur.type == T_SQRT) {
        match(T_SQRT);
        match(T_LEFTPAREN);
        Exp();
        match(T_RIGHTPAREN);
    } else {
        match(T_LEFTPAREN);
        Exp();
        match(T_RIGHTPAREN);
    }
}

// Cnd -> Exp Rel Exp
static void Cnd(void) {
    Exp();
    Rel();
    Exp();
}

// Rel -> < | = | > | <= | != | >=
//   Anything else is "Missing relational operator".
static void Rel(void) {
    switch (cur.type) {
    case T_LESSTHAN: case T_EQUAL:    case T_GREATERTHAN:
    case T_LTEQUAL:  case T_NOTEQUAL: case T_GTEQUAL:
        advance();
        break;
    default:
        parse_error("Missing relational operator");
    }
}
